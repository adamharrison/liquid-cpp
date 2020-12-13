// This implementation completely stolen shamelessly from https://metacpan.org/pod/CSS::Minifier::XS.
// All credit for this file to them.
#include <string>
#include <cstring>
#include <cstdlib>
#include <cctype>

namespace LiquidCSS {

    /* ****************************************************************************
     * CHARACTER CLASS METHODS
     * ****************************************************************************
     */
    int charIsSpace(char ch) {
        if (ch == ' ')  return 1;
        if (ch == '\t') return 1;
        return 0;
    }
    int charIsEndspace(char ch) {
        if (ch == '\n') return 1;
        if (ch == '\r') return 1;
        if (ch == '\f') return 1;
        return 0;
    }
    int charIsWhitespace(char ch) {
        return charIsSpace(ch) || charIsEndspace(ch);
    }
    int charIsIdentifier(char ch) {
        if ((ch >= 'a') && (ch <= 'z')) return 1;
        if ((ch >= 'A') && (ch <= 'Z')) return 1;
        if ((ch >= '0') && (ch <= '9')) return 1;
        if (ch == '_')  return 1;
        if (ch == '.')  return 1;
        if (ch == '#')  return 1;
        if (ch == '@')  return 1;
        if (ch == '%')  return 1;
        return 0;
    }
    int charIsInfix(char ch) {
        /* WS before+after these characters can be removed */
        if (ch == '{')  return 1;
        if (ch == '}')  return 1;
        if (ch == ';')  return 1;
        if (ch == ':')  return 1;
        if (ch == ',')  return 1;
        if (ch == '~')  return 1;
        if (ch == '>')  return 1;
        return 0;
    }
    int charIsPrefix(char ch) {
        /* WS after these characters can be removed */
        if (ch == '(')  return 1;   /* requires leading WS when used in @media */
        return charIsInfix(ch);
    }
    int charIsPostfix(char ch) {
        /* WS before these characters can be removed */
        if (ch == ')')  return 1;   /* requires trailing WS for MSIE */
        return charIsInfix(ch);
    }

    /* ****************************************************************************
     * TYPE DEFINITIONS
     * ****************************************************************************
     */
    typedef enum {
        NODE_EMPTY,
        NODE_WHITESPACE,
        NODE_BLOCKCOMMENT,
        NODE_IDENTIFIER,
        NODE_LITERAL,
        NODE_SIGIL
    } NodeType;

    struct _Node;
    typedef struct _Node Node;
    struct _Node {
        /* linked list pointers */
        Node*       prev;
        Node*       next;
        /* node internals */
        char*       contents;
        size_t      length;
        NodeType    type;
        int         can_prune;
    };

    typedef struct {
        /* linked list pointers */
        Node*       head;
        Node*       tail;
        /* doc internals */
        const char* buffer;
        size_t      length;
        size_t      offset;
    } CssDoc;

    /* ****************************************************************************
     * NODE CHECKING MACROS/FUNCTIONS
     * ****************************************************************************
     */

    /* checks to see if the node is the given str, case INSENSITIVELY */
    int nodeEquals(Node* node, const char* str) {
        return (strcasecmp(node->contents, str) == 0);
    }

    /* checks to see if the node contains the given str, case INSENSITIVELY */
    int nodeContains(Node* node, const char* str) {
        const char* haystack = node->contents;
        size_t len = strlen(str);
        char ul_start[2] = { (char)tolower(*str), (char)toupper(*str) };

        /* if node is shorter we know we're not going to have a match */
        if (len > node->length)
            return 0;

        /* find the needle in the haystack */
        while (haystack && *haystack) {
            /* find first char of needle */
            haystack = strpbrk( haystack, ul_start );
            if (haystack == NULL)
                return 0;
            /* check if the rest matches */
            if (strncasecmp(haystack, str, len) == 0)
                return 1;
            /* nope, move onto next character in the haystack */
            haystack ++;
        }

        /* no match */
        return 0;
    }

    /* checks to see if the node begins with the given str, case INSENSITIVELY.
     */
    int nodeBeginsWith(Node* node, const char* str) {
        size_t len = strlen(str);
        if (len > node->length)
            return 0;
        return (strncasecmp(node->contents, str, len) == 0);
    }

    /* checks to see if the node ends with the given str, case INSENSITVELY. */
    int nodeEndsWith(Node* node, const char* str) {
        size_t len = strlen(str);
        size_t off = node->length - len;
        if (len > node->length)
            return 0;
        return (strncasecmp(node->contents+off, str, len) == 0);
    }

    /* macros to help see what kind of node we've got */
    #define nodeIsWHITESPACE(node)          ((node->type == NODE_WHITESPACE))
    #define nodeIsBLOCKCOMMENT(node)        ((node->type == NODE_BLOCKCOMMENT))
    #define nodeIsIDENTIFIER(node)          ((node->type == NODE_IDENTIFIER))
    #define nodeIsLITERAL(node)             ((node->type == NODE_LITERAL))
    #define nodeIsSIGIL(node)               ((node->type == NODE_SIGIL))

    #define nodeIsEMPTY(node)               ((node->type == NODE_EMPTY) || ((node->length==0) || (node->contents==NULL)))
    #define nodeIsMACIECOMMENTHACK(node)    (nodeIsBLOCKCOMMENT(node) && nodeEndsWith(node,"\\*/"))
    #define nodeIsPREFIXSIGIL(node)         (nodeIsSIGIL(node) && charIsPrefix(node->contents[0]))
    #define nodeIsPOSTFIXSIGIL(node)        (nodeIsSIGIL(node) && charIsPostfix(node->contents[0]))
    #define nodeIsCHAR(node,ch)             ((node->contents[0]==ch) && (node->length==1))


    /* ****************************************************************************
     * NODE MANIPULATION FUNCTIONS
     * ****************************************************************************
     */
    /* allocates a new node */
    Node* CssAllocNode() {
        Node* node = new Node;
        node->prev = NULL;
        node->next = NULL;
        node->contents = NULL;
        node->length = 0;
        node->type = NODE_EMPTY;
        node->can_prune = 1;
        return node;
    }

    /* frees the memory used by a node */
    void CssFreeNode(Node* node) {
        if (node->contents)
            delete[] node->contents;
        delete node;
    }
    void CssFreeNodeList(Node* head) {
        while (head) {
            Node* tmp = head->next;
            CssFreeNode(head);
            head = tmp;
        }
    }

    /* clears the contents of a node */
    void CssClearNodeContents(Node* node) {
        if (node->contents)
            delete[] node->contents;
        node->contents = NULL;
        node->length = 0;
    }

    /* sets the contents of a node */
    void CssSetNodeContents(Node* node, const char* str, size_t len) {
        CssClearNodeContents(node);
        node->length = len;
        /* allocate str, fill with NULLs, and copy */
        node->contents = new char[len+1];
        memset(node->contents, 0, len+1);
        strncpy( node->contents, str, len );
    }

    /* removes the node from the list and discards it entirely */
    void CssDiscardNode(Node* node) {
        if (node->prev)
            node->prev->next = node->next;
        if (node->next)
            node->next->prev = node->prev;
        CssFreeNode(node);
    }

    /* appends the node to the given element */
    void CssAppendNode(Node* element, Node* node) {
        if (element->next)
            element->next->prev = node;
        node->next = element->next;
        node->prev = element;
        element->next = node;
    }

    /* collapses a node to a single whitespace character.  If the node contains any
     * endspace characters, that is what we're collapsed to.
     */
    void CssCollapseNodeToWhitespace(Node* node) {
        if (node->contents) {
            char ws = node->contents[0];
            size_t idx;
            for (idx=0; idx<node->length; idx++) {
                if (charIsEndspace(node->contents[idx])) {
                    ws = node->contents[idx];
                    break;
                }
            }
            CssSetNodeContents(node, &ws, 1);
        }
    }

    /* ****************************************************************************
     * TOKENIZING FUNCTIONS
     * ****************************************************************************
     */

    /* extracts a quoted literal str */
    void _CssExtractLiteral(CssDoc* doc, Node* node) {
        const char* buf = doc->buffer;
        size_t offset   = doc->offset;
        char delimiter  = buf[offset];
        /* skip start of literal */
        offset ++;
        /* search for end of literal */
        while (offset < doc->length) {
            if (buf[offset] == '\\') {
                /* escaped character; skip */
                offset ++;
            }
            else if (buf[offset] == delimiter) {
                const char* start = buf + doc->offset;
                size_t length     = offset - doc->offset + 1;
                CssSetNodeContents(node, start, length);
                node->type = NODE_LITERAL;
                return;
            }
            /* move onto next character */
            offset ++;
        }
        // croak( "unterminated quoted str literal" );
    }

    /* extracts a block comment */
    void _CssExtractBlockComment(CssDoc* doc, Node* node) {
        const char* buf = doc->buffer;
        size_t offset   = doc->offset;

        /* skip start of comment */
        offset ++;  /* skip "/" */
        offset ++;  /* skip "*" */

        /* search for end of comment block */
        while (offset < doc->length) {
            if (buf[offset] == '*') {
                if (buf[offset+1] == '/') {
                    const char* start = buf + doc->offset;
                    size_t length     = offset - doc->offset + 2;
                    CssSetNodeContents(node, start, length);
                    node->type = NODE_BLOCKCOMMENT;
                    return;
                }
            }
            /* move onto next character */
            offset ++;
        }

        // croak( "unterminated block comment" );
    }

    /* extracts a run of whitespace characters */
    void _CssExtractWhitespace(CssDoc* doc, Node* node) {
        const char* buf = doc->buffer;
        size_t offset   = doc->offset;
        while ((offset < doc->length) && charIsWhitespace(buf[offset]))
            offset ++;
        CssSetNodeContents(node, doc->buffer+doc->offset, offset-doc->offset);
        node->type = NODE_WHITESPACE;
    }

    /* extracts an identifier */
    void _CssExtractIdentifier(CssDoc* doc, Node* node) {
        const char* buf = doc->buffer;
        size_t offset   = doc->offset;
        while ((offset < doc->length) && charIsIdentifier(buf[offset]))
            offset++;
        CssSetNodeContents(node, doc->buffer+doc->offset, offset-doc->offset);
        node->type = NODE_IDENTIFIER;
    }

    /* extracts a -single- symbol/sigil */
    void _CssExtractSigil(CssDoc* doc, Node* node) {
        CssSetNodeContents(node, doc->buffer+doc->offset, 1);
        node->type = NODE_SIGIL;
    }

    /* tokenizes the given str and returns the list of nodes */
    Node* CssTokenizestr(const char* str) {
        CssDoc doc;

        /* initialize our CSS document object */
        doc.head = NULL;
        doc.tail = NULL;
        doc.buffer = str;
        doc.length = strlen(str);
        doc.offset = 0;

        /* parse the CSS */
        while ((doc.offset < doc.length) && (doc.buffer[doc.offset])) {
            /* allocate a new node */
            Node* node = CssAllocNode();
            if (!doc.head)
                doc.head = node;
            if (!doc.tail)
                doc.tail = node;

            /* parse the next node out of the CSS */
            if ((doc.buffer[doc.offset] == '/') && (doc.buffer[doc.offset+1] == '*'))
                _CssExtractBlockComment(&doc, node);
            else if ((doc.buffer[doc.offset] == '"') || (doc.buffer[doc.offset] == '\''))
                _CssExtractLiteral(&doc, node);
            else if (charIsWhitespace(doc.buffer[doc.offset]))
                _CssExtractWhitespace(&doc, node);
            else if (charIsIdentifier(doc.buffer[doc.offset]))
                _CssExtractIdentifier(&doc, node);
            else
                _CssExtractSigil(&doc, node);

            /* move ahead to the end of the parsed node */
            doc.offset += node->length;

            /* add the node to our list of nodes */
            if (node != doc.tail)
                CssAppendNode(doc.tail, node);
            doc.tail = node;
        }

        /* return the node list */
        return doc.head;
    }

    /* ****************************************************************************
     * MINIFICATION FUNCTIONS
     * ****************************************************************************
     */

    /* checks to see if the str represents a "zero unit" */
    int CssIsZeroUnit(char* str) {
        char* ptr = str;
        int foundZero = 0;

        /* Find and skip over any leading zero value */
        while (*ptr == '0') {   /* leading zeros */
            foundZero ++;
            ptr++;
        }
        if (*ptr == '.') {      /* decimal point */
            ptr++;
        }
        while (*ptr == '0') {   /* following zeros */
            foundZero ++;
            ptr++;
        }

        /* If we didn't find a zero, this isn't a Zero Unit */
        if (!foundZero) {
            return 0;
        }

        /* If it ends with a known Unit, its a Zero Unit */
        if (0 == strcmp(ptr, "em"))   { return 1; }
        if (0 == strcmp(ptr, "ex"))   { return 1; }
        if (0 == strcmp(ptr, "ch"))   { return 1; }
        if (0 == strcmp(ptr, "rem"))  { return 1; }
        if (0 == strcmp(ptr, "vw"))   { return 1; }
        if (0 == strcmp(ptr, "vh"))   { return 1; }
        if (0 == strcmp(ptr, "vmin")) { return 1; }
        if (0 == strcmp(ptr, "vmax")) { return 1; }
        if (0 == strcmp(ptr, "cm"))   { return 1; }
        if (0 == strcmp(ptr, "mm"))   { return 1; }
        if (0 == strcmp(ptr, "in"))   { return 1; }
        if (0 == strcmp(ptr, "px"))   { return 1; }
        if (0 == strcmp(ptr, "pt"))   { return 1; }
        if (0 == strcmp(ptr, "pc"))   { return 1; }
        if (0 == strcmp(ptr, "%"))    { return 1; }

        /* Nope, str contains something else; its not a Zero Unit */
        return 0;
    }

    /* collapses all of the nodes to their shortest possible representation */
    void CssCollapseNodes(Node* curr) {
        int inMacIeCommentHack = 0;
        while (curr) {
            Node* next = curr->next;
            switch (curr->type) {
                case NODE_WHITESPACE:
                    CssCollapseNodeToWhitespace(curr);
                    break;
                case NODE_BLOCKCOMMENT: {
                    if (!inMacIeCommentHack && nodeIsMACIECOMMENTHACK(curr)) {
                        /* START of mac/ie hack */
                        CssSetNodeContents(curr, "/*\\*/", 5);
                        curr->can_prune = 0;
                        inMacIeCommentHack = 1;
                    }
                    else if (inMacIeCommentHack && !nodeIsMACIECOMMENTHACK(curr)) {
                        /* END of mac/ie hack */
                        CssSetNodeContents(curr, "/**/", 4);
                        curr->can_prune = 0;
                        inMacIeCommentHack = 0;
                    }
                    } break;
                case NODE_IDENTIFIER:
                    if (CssIsZeroUnit(curr->contents)) {
                        CssSetNodeContents(curr, "0", 1);
                    }
                default:
                    break;
            }
            curr = next;
        }
    }

    /* checks to see whether we can prune the given node from the list.
     *
     * THIS is the function that controls the bulk of the minification process.
     */
    enum {
        PRUNE_NO,
        PRUNE_PREVIOUS,
        PRUNE_CURRENT,
        PRUNE_NEXT
    };
    int CssCanPrune(Node* node) {
        Node* prev = node->prev;
        Node* next = node->next;

        /* only if node is prunable */
        if (!node->can_prune)
            return PRUNE_NO;

        switch (node->type) {
            case NODE_EMPTY:
                /* prune empty nodes */
                return PRUNE_CURRENT;
            case NODE_WHITESPACE:
                /* remove whitespace before comment blocks */
                if (next && nodeIsBLOCKCOMMENT(next))
                    return PRUNE_CURRENT;
                /* remove whitespace after comment blocks */
                if (prev && nodeIsBLOCKCOMMENT(prev))
                    return PRUNE_CURRENT;
                /* leading whitespace gets pruned */
                if (!prev)
                    return PRUNE_CURRENT;
                /* trailing whitespace gets pruned */
                if (!next)
                    return PRUNE_CURRENT;
                /* keep all other whitespace */
                return PRUNE_NO;
            case NODE_BLOCKCOMMENT:
                /* keep comments that contain the word "copyright" */
                if (nodeContains(node,"copyright"))
                    return PRUNE_NO;
                /* remove comment blocks */
                return PRUNE_CURRENT;
            case NODE_IDENTIFIER:
                /* keep all identifiers */
                return PRUNE_NO;
            case NODE_LITERAL:
                /* keep all literals */
                return PRUNE_NO;
            case NODE_SIGIL:
                /* remove whitespace after "prefix" sigils */
                if (nodeIsPREFIXSIGIL(node) && next && nodeIsWHITESPACE(next))
                    return PRUNE_NEXT;
                /* remove whitespace before "postfix" sigils */
                if (nodeIsPOSTFIXSIGIL(node) && prev && nodeIsWHITESPACE(prev))
                    return PRUNE_PREVIOUS;
                /* remove ";" characters at end of selector groups */
                if (nodeIsCHAR(node,';') && next && nodeIsSIGIL(next) && nodeIsCHAR(next,'}'))
                    return PRUNE_CURRENT;
                /* keep all other sigils */
                return PRUNE_NO;
        }
        /* keep anything else */
        return PRUNE_NO;
    }

    /* prune nodes from the list */
    Node* CssPruneNodes(Node *head) {
        Node* curr = head;
        while (curr) {
            /* see if/how we can prune this node */
            int prune = CssCanPrune(curr);
            /* prune.  each block is responsible for moving onto the next node */
            Node* prev = curr->prev;
            Node* next = curr->next;
            switch (prune) {
                case PRUNE_PREVIOUS:
                    /* discard previous node */
                    CssDiscardNode(prev);
                    /* reset "head" if that's what got pruned */
                    if (prev == head)
                        head = curr;
                    break;
                case PRUNE_CURRENT:
                    /* discard current node */
                    CssDiscardNode(curr);
                    /* reset "head" if that's what got pruned */
                    if (curr == head)
                        head = prev ? prev : next;
                    /* backup and try again if possible */
                    curr = prev ? prev : next;
                    break;
                case PRUNE_NEXT:
                    /* discard next node */
                    CssDiscardNode(next);
                    /* stay on current node, and try again */
                    break;
                default:
                    /* move ahead to next node */
                    curr = next;
                    break;
            }
        }

        /* return the (possibly new) head node back to the caller */
        return head;
    }

    /* ****************************************************************************
     * Minifies the given CSS, returning a newly allocated str back to the
     * caller (YOU'RE responsible for freeing its memory).
     * ****************************************************************************
     */
    std::string CssMinify(const std::string& str) {
        std::string results;
        /* PASS 1: tokenize CSS into a list of nodes */
        Node* head = CssTokenizestr(str.c_str());
        if (!head) return NULL;
        /* PASS 2: collapse nodes */
        CssCollapseNodes(head);
        /* PASS 3: prune nodes */
        head = CssPruneNodes(head);
        if (!head)
            return std::string();
        char* ptr;
        /* PASS 4: re-assemble CSS into single str */
        {
            Node* curr;
            /* allocate the result buffer to the same size as the original CSS; in
             * a worst case scenario that's how much memory we'll need for it.
             */
            results.resize(str.size()+1);
            ptr = &results[0];
            /* copy node contents into result buffer */
            curr = head;
            while (curr) {
                memcpy(ptr, curr->contents, curr->length);
                ptr += curr->length;
                curr = curr->next;
            }
            *ptr = 0;
        }
        /* free memory used by node list */
        CssFreeNodeList(head);
        /* return resulting minified CSS back to caller */
        results.resize(ptr - &results[0]);
        return results;
    }
}

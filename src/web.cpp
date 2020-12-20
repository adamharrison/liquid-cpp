#include "web.h"
#include "context.h"
#include <sass/context.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>

#include "css_minifier_xs.cpp"
#include "js_minifier_xs.cpp"

namespace Liquid {

    struct MinifyJSNode : TagNodeType {
        MinifyJSNode() : TagNodeType(Composition::ENCLOSED, "minify_js", 0, 1) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            return Variant(LiquidJS::JsMinify(renderer.retrieveRenderedNode(*node.children.back().get(), store).getString()));
        }
    };

    struct MinifyCSSNode : TagNodeType {
        MinifyCSSNode() : TagNodeType(Composition::ENCLOSED, "minify_css", 0, 1) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            return Variant(LiquidCSS::CssMinify(renderer.retrieveRenderedNode(*node.children.back().get(), store).getString()));
        }
    };

    struct MinifyHTMLNode : TagNodeType {
        MinifyHTMLNode() : TagNodeType(Composition::ENCLOSED, "minify_html", 0, 1) { }

        Node render(Renderer& renderer, const Node& node, Variable store) const {
            return renderer.retrieveRenderedNode(*node.children.back().get(), store);
        }
    };

    struct SCSSNode : TagNodeType {
        SCSSNode() : TagNodeType(Composition::ENCLOSED, "scss", 0, 1) { }

        Node render(Renderer& renderer, const Node& node, Variable store) const {
            string s = renderer.retrieveRenderedNode(*node.children.back().get(), store).getString();

            char* text = sass_copy_c_string(s.c_str());
            struct Sass_Data_Context* data_ctx = sass_make_data_context(text);
            struct Sass_Context* ctx = sass_data_context_get_context(data_ctx);
            // context is set up, call the compile step now
            int status = sass_compile_data_context(data_ctx);
            if (status != 0) {
                sass_delete_data_context(data_ctx);
                return Node();
            }
            string result = string(sass_context_get_output_string(ctx));
            sass_delete_data_context(data_ctx);
            return Variant(move(result));
        }
    };

    struct EscapeFilterNode : FilterNodeType {
        EscapeFilterNode() : FilterNodeType("escape", 0, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            string incoming = renderer.retrieveRenderedNode(*node.children.front().get(), store).getString();
            string result;
            result.reserve(int(incoming.size()*1.10));
            for (size_t i = 0; i < incoming.size(); ++i) {
                char c = incoming[i];
                switch (c) {
                    case '\'':
                        result += "&apos;";
                    break;
                    case '"':
                        result += "&quot;";
                    break;
                    case '<':
                        result += "&lt;";
                    break;
                    case '>':
                        result += "&gt;";
                    break;
                    case '&':
                        result += "&amp;";
                    break;
                    default:
                        result += incoming[i];
                }
            }
            return Variant(move(result));
        }
    };

    void toHex(char* dst, const unsigned char* src, int size) {
        static const char hexDigits[] = "0123456789abcdef";
        for (int i = 0; i < size; ++i) {
            *(dst++) = hexDigits[src[i] >> 4];
            *(dst++) = hexDigits[src[i] & 0xF];
        }
    }

    template <unsigned char* (*FUNC)(const unsigned char*, size_t, unsigned char*), int LENGTH>
    struct DigestFilterNode : FilterNodeType {
        DigestFilterNode(const string& symbol) : FilterNodeType(symbol, 0, 0) { }

        Node render(Renderer& renderer, const Node& node, Variable store) const {
            string incoming = getOperand(renderer, node, store).getString();
            unsigned char c[LENGTH];
            string result;
            result.resize(LENGTH*2);
            FUNC((unsigned char*)incoming.data(), incoming.size(), c);
            toHex(&result[0], c, LENGTH);
            return Variant(move(result));
        }
    };

    struct DigestHMACFilterNode : FilterNodeType {
        DigestHMACFilterNode(const string& symbol) : FilterNodeType(symbol, 1, 1) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            string incoming = getOperand(renderer, node, store).getString();
            string key = getArgument(renderer, node, store, 0).getString();
            unsigned char c[1024];
            string result;
            unsigned int len;
            const EVP_MD* func = EVP_get_digestbyname(&symbol.c_str()[5]);
            HMAC(func, key.data(), key.size(), (const unsigned char*)incoming.data(), incoming.size(), c, &len);
            result.resize(len*2);
            toHex(&result[0], c, len);
            return Variant(move(result));
        }
    };

    struct MD5FilterNode : DigestFilterNode<MD5, MD5_DIGEST_LENGTH> { MD5FilterNode() : DigestFilterNode<MD5, MD5_DIGEST_LENGTH>("md5") { } };
    struct SHA1FilterNode : DigestFilterNode<SHA1, SHA_DIGEST_LENGTH> { SHA1FilterNode() : DigestFilterNode<SHA1, SHA_DIGEST_LENGTH>("sha1") { } };
    struct SHA256FilterNode : DigestFilterNode<SHA256, SHA256_DIGEST_LENGTH> { SHA256FilterNode() : DigestFilterNode<SHA256, SHA256_DIGEST_LENGTH>("sha256") { } };
    struct HMACSHA1FilterNode : DigestHMACFilterNode { HMACSHA1FilterNode() : DigestHMACFilterNode("hmac_sha1") { } };
    struct HMACSHA256FilterNode : DigestHMACFilterNode { HMACSHA256FilterNode() : DigestHMACFilterNode("hmac_sha256") { } };

    void WebDialect::implement(Context& context) {
        context.registerType<MinifyJSNode>();
        context.registerType<MinifyCSSNode>();
        context.registerType<MinifyHTMLNode>();
        context.registerType<SCSSNode>();
        context.registerType<EscapeFilterNode>();
        context.registerType<MD5FilterNode>();
        context.registerType<SHA1FilterNode>();
        context.registerType<SHA256FilterNode>();
        context.registerType<HMACSHA1FilterNode>();
        context.registerType<HMACSHA256FilterNode>();
    }
}

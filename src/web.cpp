#include "web.h"
#include "context.h"

#if LIQUID_INCLUDE_WEB_DIALECT

#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <cmath>

namespace Liquid {

    struct EscapeFilterNode : FilterNodeType {
        static string htmlEscape(const string& incoming) {
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
                        result += c;
                }
            }
            return result;
        }

        EscapeFilterNode() : FilterNodeType("escape", 0, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            return Variant(htmlEscape(renderer.retrieveRenderedNode(*node.children.front().get(), store).getString()));
        }
    };

    static const char hexDigits[] = "0123456789abcdef";

    struct URLEncodeFilterNode : FilterNodeType {
        static string paramEncode(const string& incoming) {
            string result;
            result.reserve(int(incoming.size()*1.10));
            for (size_t i = 0; i < incoming.size(); ++i) {
                char c = incoming[i];
                if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z'))
                    result += c;
                else {
                    result += hexDigits[c >> 4];
                    result += hexDigits[c & 0xF];
                }
            }
            return result;
        }

        URLEncodeFilterNode() : FilterNodeType("url_encode", 0, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            return Variant(paramEncode(renderer.retrieveRenderedNode(*node.children.front().get(), store).getString()));
        }
    };


    void toHex(char* dst, const unsigned char* src, int size) {
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


    struct LinkToFilterNode : FilterNodeType {
        LinkToFilterNode() : FilterNodeType("link_to", 1, 2) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            string title = getArgument(renderer, node, store, 1).getString();
            string url = "<a " + (!title.empty() ? ("title=\"" + EscapeFilterNode::htmlEscape(title) + "\"") : "") + " href=\"" + getArgument(renderer, node, store, 0).getString() + "\">" + EscapeFilterNode::htmlEscape(getOperand(renderer, node, store).getString()) + "</a>";
            return Variant(move(url));
        }
    };

    struct ImgTagFilterNode : FilterNodeType {
        ImgTagFilterNode() : FilterNodeType("img_tag", 0, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            return Variant(string("<img src='") + getOperand(renderer, node, store).getString() + "'/>");
        }
    };
    struct StylesheetTagFilterNode : FilterNodeType {
        StylesheetTagFilterNode() : FilterNodeType("stylesheet_tag", 0, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            return Variant(string("<link href=\"") + getOperand(renderer, node, store).getString() + "\" rel=\"stylesheet\" type=\"text/css\" media=\"all\" />");
        }
    };
    struct ScriptTagFilterNode : FilterNodeType {
        ScriptTagFilterNode() : FilterNodeType("script_tag", 0, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            return Variant(string("<script src=\"") + getOperand(renderer, node, store).getString() + "\" type=\"text/javascript\"></script>");
        }
    };
    struct HighlightFilterNode : FilterNodeType {
        HighlightFilterNode() : FilterNodeType("highlight", 1, 1) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            string operand = getOperand(renderer, node, store).getString();
            string argument = getArgument(renderer, node, store, 0).getString();
            string result;
            size_t offset = 0, target;
            bool hasOne = false;
            while ((target = operand.find(argument, offset)) != string::npos) {
                if (hasOne)
                    result.append("</strong>");
                result.append(operand, offset, target - offset);
                result.append("<strong>");
                hasOne = true;
            }
            if (hasOne)
                result.append("</strong>");
            result.append(operand, offset, operand.size() - offset);
            return Variant(move(result));
        }
    };
    struct DateFilterNode : FilterNodeType {
        DateFilterNode() : FilterNodeType("date", 1, 1) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            time_t operand = (time_t)getOperand(renderer, node, store).variant.getInt();
            string argument = getArgument(renderer, node, store, 0).getString();
            struct tm * timeinfo = localtime(&operand);
            static constexpr int MAX_BUFFER_SIZE = 256;
            string buffer;
            buffer.resize(MAX_BUFFER_SIZE);
            buffer.resize(strftime(&buffer[0], MAX_BUFFER_SIZE, argument.c_str(), timeinfo));
            return Variant(move(buffer));
        }
    };

    struct ColorFilterNode : FilterNodeType {
        ColorFilterNode(const string& symbol, int minArguments, int maxArguments) : FilterNodeType(symbol, minArguments, maxArguments) { }

        unsigned int parseHex(const std::string& hex) const {
            size_t offset = 0;
            if (hex.size() == 0)
                return 0;
            if (hex[0] == '#')
                offset = 1;
            return std::stoul(hex, &offset, 16);
        }

        string printHex(unsigned int color) const {
            char buffer[8] = "#000000";
            char chrs[] = "0123456789ABCDEF";
            for (int i = 0; i < 6; ++i)
                buffer[i+1] = chrs[(color >> ((24 - (4*i)) + 8)) & 0xF];
            return string(buffer, 7);
        }

        string printRGB(unsigned int color) const {
            char buffer[128];
            sprintf(buffer, "rgb(%d, %d, %d)", getRed(color), getGreen(color), getBlue(color));
            return string(buffer);
        }

        string printRGBA(unsigned int color) const {
            char buffer[128];
            sprintf(buffer, "rgba(%d, %d, %d, %f)", getRed(color), getGreen(color), getBlue(color), (getAlpha(color)/255.0f));
            return string(buffer);
        }

        string printHSL(unsigned int color) const {
            char buffer[128];
            HSL hsl = getHSL(color);
            sprintf(buffer, "hsl(%f, %f%%, %f%%)", hsl.hue * 360.0f, hsl.saturation * 100.0f, hsl.luminance * 100.0f);
            return string(buffer);
        }

        unsigned char getRed(unsigned int color) const { return (unsigned char)((color >> 24) & 0xFF); }
        unsigned char getGreen(unsigned int color) const { return (unsigned char)((color >> 16) & 0xFF); }
        unsigned char getBlue(unsigned int color) const { return (unsigned char)((color >> 8) & 0xFF); }
        unsigned char getAlpha(unsigned int color) const { return (unsigned char)(color & 0xFF); }

        struct HSL {
            float hue;
            float saturation;
            float luminance;
        };

        HSL getHSL(unsigned int color) const {
            float pixels[3] = { (float)getRed(color)/255.0f, (float)getGreen(color)/255.0f, (float)getBlue(color)/255.0f };

            float maximum;
            float hue;
            float minimum = std::min(std::min(pixels[0], pixels[1]), pixels[2]);
            if (pixels[0] > pixels[1]) {
                if (pixels[0] > pixels[2]) {
                    maximum = pixels[0];
                    hue = (pixels[1] - pixels[2]) / (maximum - minimum);
                } else {
                    maximum = pixels[2];
                    hue = 4.0f + ((pixels[0] - pixels[1]) / (maximum - minimum));
                }
            } else {
                if (pixels[1] > pixels[2]) {
                    maximum = pixels[1];
                    hue = 2.0f + ((pixels[2] - pixels[0]) / (maximum - minimum));
                } else {
                    maximum = pixels[2];
                    hue = 4.0f + ((pixels[0] - pixels[1]) / (maximum - minimum));
                }
            }
            hue *= 60.0f;
            if (hue < 0.0f)
                hue += 180.0f;

            float luminance = (maximum + minimum) / 2.0f;
            float saturation;
            if (luminance > 0.5f) {
                saturation = (maximum - minimum) / (2.0f - maximum - minimum);
            } else {
                saturation = (maximum - minimum) / (maximum + minimum);
            }

            return HSL { hue, saturation, luminance };
        }

        unsigned int getRGB(HSL hsl, int alpha = 0) const {
            int pixels[4] = { 0, 0, 0, alpha };

            float temp1 = hsl.luminance < 0.5f ? hsl.luminance * (1.0f + hsl.saturation) : (hsl.luminance + hsl.saturation) - hsl.luminance*hsl.saturation;
            float temp2 = 2 * hsl.luminance - temp1;

            float tempColors[3] = { hsl.hue + 0.3333333f, hsl.hue, hsl.hue - 0.3333333f };
            for (int i = 0; i < 3; ++i) {
                float color = tempColors[i];
                if (color < 0.0f)
                    color += 1.0f;
                else if (color > 1.0f)
                    color -= 1.0f;
                if (color * 6 < 1.0f)
                    pixels[i] = temp2 + (temp1 - temp2) * 6.0f * color;
                else if (color * 2 < 1.0f)
                    pixels[i] = temp1;
                else if (color * 3 < 2.0f)
                    pixels[i] = temp2 + (temp1 - temp2) * (0.666666f - color) * 6.0f;
                else
                    pixels[i] = temp2;
            }

            return getRGB(pixels[0], pixels[1], pixels[2], pixels[3]);
        }

        unsigned int getRGB(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255) const {
            return (r << 24) | (g << 16) | (b << 8) | a;
        }

        float getBrightness(unsigned int color) const {
            return (getRed(color) * 299 + getGreen(color) * 587 + getBlue(color) * 114) / 1000.0f;
        }

        unsigned int getColorString(const std::string& operand) const {
            if (operand[0] == '#')
                return parseHex(operand);
            if (operand.find("rgb") == 0) {
                int pixels[4] = { 0, 0, 0, 255 };
                if (operand.size() > 3 && operand[3] == 'a') {
                    float percent;
                    sscanf(operand.c_str(), "rgba(%d, %d, %d, %f)", &pixels[0], &pixels[1], &pixels[2], &percent);
                    pixels[3] = (int)(percent * 255.0f);
                } else
                    sscanf(operand.c_str(), "rgb(%d, %d, %d)", &pixels[0], &pixels[1], &pixels[2]);
                return getRGB(pixels[0], pixels[1], pixels[2], pixels[3]);
            } else if (operand.find("hsl") == 0) {
                float hue;
                float saturation;
                float luminance;
                int pixels[4] = { 0, 0, 0, 0 };
                if (operand.size() > 3 && operand[3] == 'a') {
                    float percent;
                    sscanf(operand.c_str(), "hsla(%f, %f%%, %f%%, %f)", &hue, &saturation, &luminance, &percent);
                    pixels[3] = (int)(percent * 255.0f);
                } else
                    sscanf(operand.c_str(), "hsl(%f, %f%%, %f%%)", &hue, &saturation, &luminance);

                hue /= 360.0f;
                saturation /= 100.0f;
                luminance /= 100.0f;

                return getRGB(HSL { hue, saturation, luminance }, pixels[3]);
            }
            return 0;
        }

        unsigned int getColorOperand(Renderer& renderer, const Node& node, Variable store) const {
            return getColorString(getOperand(renderer, node, store).getString());
        }
    };

    struct ColorToRGBFilterNode : ColorFilterNode {
        ColorToRGBFilterNode() : ColorFilterNode("color_to_rgb", 0, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            return Variant(printRGB(getColorOperand(renderer, node, store)));
        }
    };

    struct ColorToHSLFilterNode : ColorFilterNode {
        ColorToHSLFilterNode() : ColorFilterNode("color_to_hsl", 0, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            return Variant(printHSL(getColorOperand(renderer, node, store)));
        }
    };

    struct ColorToHexFilterNode : ColorFilterNode {
        ColorToHexFilterNode() : ColorFilterNode("color_to_hex", 0, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            return Variant(printHex(getColorOperand(renderer, node, store)));
        }
    };


    struct ColorExtractFilterNode : ColorFilterNode {
        ColorExtractFilterNode() : ColorFilterNode("color_extract", 1, 1) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            unsigned int color = getColorOperand(renderer, node, store);
            string op = getArgument(renderer, node, store, 0).getString();
            if (op == "red")
                return Variant((long long)getRed(color));
            else if (op == "green")
                return Variant((long long)getGreen(color));
            else if (op == "blue")
                return Variant((long long)getBlue(color));
            else if (op == "alpha")
                return Variant(getAlpha(color) / 255.0f);
            else if (op == "hue")
                return Variant((long long)(getHSL(color).hue*360.0f));
            else if (op == "saturation")
                return Variant(getHSL(color).saturation*100.0f);
            else if (op == "lightness")
                return Variant(getHSL(color).luminance*100.0f);
            return Node();
        }
    };

    struct ColorBrightnessFilterNode : ColorFilterNode {
        ColorBrightnessFilterNode() : ColorFilterNode("color_brightness", 0, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            return Variant(getBrightness(getColorOperand(renderer, node, store)));
        }
    };

    struct ColorModifyFilterNode : ColorFilterNode {
        ColorModifyFilterNode() : ColorFilterNode("color_modify", 2, 2) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            unsigned int color = getColorOperand(renderer, node, store);
            string op = getArgument(renderer, node, store, 0).getString();
            Variant v = getArgument(renderer, node, store, 1).variant;
            if (op == "red")
                return Variant(printHex((color & 0x00FFFFFF) | ((v.getInt() & 0xFF) << 24)));
            else if (op == "green")
                return Variant(printHex((color & 0xFF00FFFF) | ((v.getInt() & 0xFF) << 16)));
            else if (op == "blue")
                return Variant(printHex((color & 0xFFFF00FF) | ((v.getInt() & 0xFF) << 8)));
            else if (op == "alpha")
                return Variant(printRGBA((color & 0xFFFFFF00) | ((int)(v.getFloat()*255.0f) & 0xFF)));
            else if (op == "hue") {
                HSL hsl = getHSL(color);
                hsl.hue = (float)(v.getFloat() / 360.0f);
                return Variant(printHSL(getRGB(hsl, getAlpha(color))));
            } else if (op == "saturation") {
                HSL hsl = getHSL(color);
                hsl.saturation = (float)(v.getFloat() / 100.0f);
                return Variant(printHSL(getRGB(hsl, getAlpha(color))));
            } else if (op == "lightness") {
                HSL hsl = getHSL(color);
                hsl.luminance = (float)(v.getFloat() / 100.0f);
                return Variant(printHSL(getRGB(hsl, getAlpha(color))));
            }
            return Variant(printHex(color));
        }
    };

    struct ColorLightenFilterNode : ColorFilterNode {
        ColorLightenFilterNode() : ColorFilterNode("color_lighten", 1, 1) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            unsigned int color = getColorOperand(renderer, node, store);
            HSL hsl = getHSL(color);
            float percent = getArgument(renderer, node, store, 0).variant.getFloat() / 100.0f;
            hsl.luminance += (1.0f - hsl.luminance) * percent;
            return Variant(printHex(getRGB(hsl, getAlpha(color))));
        }
    };

    struct ColorDarkenFilterNode : ColorFilterNode {
        ColorDarkenFilterNode() : ColorFilterNode("color_darken", 1, 1) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            unsigned int color = getColorOperand(renderer, node, store);
            HSL hsl = getHSL(color);
            float percent = getArgument(renderer, node, store, 0).variant.getFloat() / 100.0f;
            hsl.luminance += (0.0f - hsl.luminance) * percent;
            return Variant(printHex(getRGB(hsl, getAlpha(color))));
        }
    };


    struct ColorSaturateFilterNode : ColorFilterNode {
        ColorSaturateFilterNode() : ColorFilterNode("color_saturate", 1, 1) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            unsigned int color = getColorOperand(renderer, node, store);
            HSL hsl = getHSL(color);
            float percent = getArgument(renderer, node, store, 0).variant.getFloat() / 100.0f;
            hsl.saturation += (1.0f - hsl.saturation) * percent;
            return Variant(printHex(getRGB(hsl, getAlpha(color))));
        }
    };

    struct ColorDesaturateFilterNode : ColorFilterNode {
        ColorDesaturateFilterNode() : ColorFilterNode("color_desaturate", 1, 1) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            unsigned int color = getColorOperand(renderer, node, store);
            HSL hsl = getHSL(color);
            float percent = getArgument(renderer, node, store, 0).variant.getFloat() / 100.0f;
            hsl.saturation += (0.0f - hsl.saturation) * percent;
            return Variant(printHex(getRGB(hsl, getAlpha(color))));
        }
    };

    struct ColorMixFilterNode : ColorFilterNode {
        ColorMixFilterNode() : ColorFilterNode("color_mix", 2, 2) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            unsigned int input = getColorOperand(renderer, node, store);
            unsigned int mix = getColorString(getArgument(renderer, node, store, 0).getString());
            float percent = getArgument(renderer, node, store, 1).variant.getFloat() / 100.0f;

            return Variant(printHex(getRGB(
                ((getRed(mix) - getRed(input)) * percent) + getRed(input),
                ((getGreen(mix) - getGreen(input)) * percent) + getGreen(input),
                ((getBlue(mix) - getBlue(input)) * percent) + getBlue(input),
                ((getAlpha(mix) - getAlpha(input)) * percent) + getAlpha(input)
            )));
        }
    };

    struct ColorContrastFilterNode : ColorFilterNode {
        ColorContrastFilterNode() : ColorFilterNode("color_contrast", 1, 1) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            unsigned int color1 = getColorOperand(renderer, node, store);
            unsigned int color2 = getColorString(getArgument(renderer, node, store, 0).getString());

            float luminance1 =  0.2126f * getRed(color1) + 0.7152 * getGreen(color1) + 0.0722 * getBlue(color1);
            float luminance2 =  0.2126f * getRed(color2) + 0.7152 * getGreen(color2) + 0.0722 * getBlue(color2);

            if (luminance1 > luminance2)
                return Variant((luminance1 + 0.05f) / (luminance2 + 0.05f));
            return Variant((luminance2 + 0.05f) / (luminance1 + 0.05f));
        }
    };


    struct ColorDifferenceFilterNode : ColorFilterNode {
        ColorDifferenceFilterNode() : ColorFilterNode("color_difference", 1, 1) { }

        float square(float f) const { return f*f; }

        Node render(Renderer& renderer, const Node& node, Variable store) const {
            unsigned int color1 = getColorOperand(renderer, node, store);
            unsigned int color2 = getColorString(getArgument(renderer, node, store, 0).getString());
            return Variant(sqrtf(square(getRed(color1) - getRed(color2)) + square(getBlue(color1) - getBlue(color2)) + square(getBlue(color1) - getBlue(color2))));
        }
    };


    struct BrightnessDifferenceFilterNode : ColorFilterNode {
        BrightnessDifferenceFilterNode() : ColorFilterNode("brightness_difference", 1, 1) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            unsigned int color1 = getColorOperand(renderer, node, store);
            unsigned int color2 = getColorString(getArgument(renderer, node, store, 0).getString());
            return Variant(abs(getBrightness(color1) - getBrightness(color2)));
        }
    };

    #if LIQUID_INCLUDE_RAPIDJSON_VARIABLE
    struct JSONFilterNode : FilterNodeType {
        JSONFilterNode() : FilterNodeType("json") { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            Node argument = getOperand(renderer, node, store);
            return argument;
        }
    };
    #endif

    void WebDialect::implement(Context& context) {
        context.registerType<EscapeFilterNode>();
        context.registerType<URLEncodeFilterNode>();
        context.registerType<MD5FilterNode>();
        context.registerType<SHA1FilterNode>();
        context.registerType<SHA256FilterNode>();
        context.registerType<HMACSHA1FilterNode>();
        context.registerType<HMACSHA256FilterNode>();

        context.registerType<ColorToRGBFilterNode>();
        context.registerType<ColorToHSLFilterNode>();
        context.registerType<ColorToHexFilterNode>();
        context.registerType<ColorExtractFilterNode>();
        context.registerType<ColorBrightnessFilterNode>();
        context.registerType<ColorModifyFilterNode>();
        context.registerType<ColorLightenFilterNode>();
        context.registerType<ColorDarkenFilterNode>();
        context.registerType<ColorSaturateFilterNode>();
        context.registerType<ColorDesaturateFilterNode>();
        context.registerType<ColorMixFilterNode>();
        context.registerType<ColorContrastFilterNode>();
        context.registerType<ColorDifferenceFilterNode>();
        context.registerType<BrightnessDifferenceFilterNode>();

        context.registerType<ImgTagFilterNode>();
        context.registerType<StylesheetTagFilterNode>();
        // context.registerType<TimeTagFilterNode>();
        context.registerType<ScriptTagFilterNode>();
        context.registerType<LinkToFilterNode>();
        context.registerType<HighlightFilterNode>();
        context.registerType<DateFilterNode>();

        #if LIQUID_INCLUDE_RAPIDJSON_VARIABLE
            context.registerType<JSONFilterNode>();
        #endif
    }
}

#endif

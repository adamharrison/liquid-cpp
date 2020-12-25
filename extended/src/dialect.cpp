
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
												//             int status = sass_compile_data_context(data_ctx);
												//                         if (status != 0) {
												//                                         sass_delete_data_context(data_ctx);
												//                                                         return Node();
												//                                                                     }
												//                                                                                 string result = string(sass_context_get_output_string(ctx));
												//                                                                                             sass_delete_data_context(data_ctx);
												//                                                                                                         return Variant(move(result));
												//                                                                                                                 }
												//                                                                                                                     };

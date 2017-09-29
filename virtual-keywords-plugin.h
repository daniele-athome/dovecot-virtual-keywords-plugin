#ifndef VIRTUAL_KEYWORDS_PLUGIN_H
#define VIRTUAL_KEYWORDS_PLUGIN_H

extern const char *virtual_keywords_plugin_dependencies[];
extern const char virtual_keywords_plugin_binary_dependency[];

void virtual_keywords_plugin_init(struct module *module);
void virtual_keywords_plugin_deinit(void);

#endif

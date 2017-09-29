
#include "config.h"
#include "lib.h"
#include "notify-plugin.h"
#include "mail-storage-private.h"

#include "virtual-keywords-plugin.h"


static void
virtual_keywords_mail_update_keywords(void *txn, struct mail *mail,
                              const char *const *old_keywords)
{
    // TODO create virtual folders for new keywords
}

static void
virtual_keywords_mail_user_created(struct mail_user *user)
{
    // TODO create default virtual folders (All, Starred, ...)
}



static const struct notify_vfuncs virtual_keywords_vfuncs = {
    .mail_update_keywords = virtual_keywords_mail_update_keywords
};

static struct notify_context *virtual_keywords_ctx;

static struct mail_storage_hooks virtual_keywords_mail_storage_hooks = {
    .mail_user_created = virtual_keywords_mail_user_created
};

void
virtual_keywords_plugin_init(struct module *module)
{
    virtual_keywords_ctx = notify_register(&virtual_keywords_vfuncs);
    mail_storage_hooks_add(module, &virtual_keywords_mail_storage_hooks);
}

void
virtual_keywords_plugin_deinit(void)
{
    mail_storage_hooks_remove(&virtual_keywords_mail_storage_hooks);
    notify_unregister(virtual_keywords_ctx);
}

const char *virtual_keywords_plugin_dependencies[] = { "notify", NULL };
const char virtual_keywords_plugin_binary_dependency[] = "imap";

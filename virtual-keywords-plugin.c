
#include "config.h"
#include "lib.h"
#include "str-sanitize.h"
#include "failures.h"
#include "ostream.h"
#include "notify-plugin.h"
#include "mail-storage-private.h"

#include "virtual-keywords-plugin.h"

#define MAILBOX_NAME_LOG_LEN 64
#define VIRTUAL_KEYWORDS_DEFAULT_PREFIX "Virtual."

#define VIRTUAL_KEYWORDS_MAILBOX_ALL    "All"
#define VIRTUAL_KEYWORDS_SEARCH_PREFIX  "KEYWORD "
#define VIRTUAL_CONFIG_FNAME            "dovecot-virtual"

#define VIRTUAL_KEYWORDS_USER_CONTEXT(obj) \
	MODULE_CONTEXT(obj, virtual_keywords_user_module)

struct virtual_keywords_user {
    union mail_user_module_context module_ctx;

    const char* prefix;
};

static MODULE_CONTEXT_DEFINE_INIT(virtual_keywords_user_module,
                                  &mail_user_module_register);


static bool
virtual_keywords_create_mailbox(struct mailbox *box, const char *name, const char *rules)
{
    struct virtual_keywords_user *muser =
            VIRTUAL_KEYWORDS_USER_CONTEXT(box->storage->user);

    struct mail_namespace *ns = mail_namespace_find_prefix(box->storage->user->namespaces, muser->prefix);
    if (ns == NULL) {
        i_error("Unable to find namespace with prefix %s. Did you configure it?", muser->prefix);
        return FALSE;
    }

    i_debug("Virtual namespace found: %s", ns->prefix);
    i_debug("Virtual namespace location: %s", ns->set->location);

    const char *label_box_name = t_strconcat(ns->prefix, name, NULL);
    struct mailbox *label_box = mailbox_alloc(ns->list, label_box_name, MAILBOX_FLAG_READONLY);

    // check for mailbox existence
    enum mailbox_existence existence;
    if (mailbox_exists(label_box, TRUE, &existence) < 0) {
        i_warning("Failed to check for mailbox existence.");
        mailbox_free(&label_box);
        return FALSE;
    }

    if (existence != MAILBOX_EXISTENCE_SELECT) {
        i_info("Virtual mailbox doesn't exist, creating it");

        const char *label_path = mailbox_get_path(label_box);
        i_debug("Virtual mailbox path is: %s", label_path);

        if (mailbox_mkdir(label_box, label_path, MAILBOX_LIST_PATH_TYPE_MAILBOX) < 0) {
            i_error("Failed to create virtual keyword mailbox (%s)",
                    mail_storage_get_last_error(label_box->storage, NULL));
            mailbox_free(&label_box);
            return FALSE;
        }

        const char *config_path = t_strconcat(label_path, "/"VIRTUAL_CONFIG_FNAME, NULL);
        int fd = open(config_path, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);
        if (fd == -1) {
            i_error("Failed to create virtual keyword configuration (%s)", strerror(errno));
            mailbox_free(&label_box);
            return FALSE;
        }

        struct ostream *os = o_stream_create_fd_file(fd, (size_t)-1, TRUE);
        ssize_t bytes = o_stream_send_str(os, rules);
        o_stream_close(os);

        if (bytes < strlen(rules)) {
            i_error("Failed to write virtual keyword configuration (%s)", o_stream_get_error(os));
            mailbox_free(&label_box);
            return FALSE;
        }
    }

    mailbox_free(&label_box);
    return TRUE;
}

static const char *
virtual_keywords_create_rule(const char *prefix, const char *name)
{
    return t_strconcat(prefix, VIRTUAL_KEYWORDS_MAILBOX_ALL, "\n  ", VIRTUAL_KEYWORDS_SEARCH_PREFIX, name, "\n", NULL);
}

static void
virtual_keywords_mail_update_keywords(void *txn, struct mail *mail,
                              const char *const *old_keywords)
{
    struct virtual_keywords_user *muser =
            VIRTUAL_KEYWORDS_USER_CONTEXT(mail->box->storage->user);

    const char *const *keywords = mail_get_keywords(mail);
    unsigned int i;

    for (i = 0; keywords[i] != NULL; i++) {
        virtual_keywords_create_mailbox(mail->box, keywords[i],
            virtual_keywords_create_rule(muser->prefix, keywords[i]));
    }
}

static void
virtual_keywords_mailbox_create(struct mailbox *box)
{
    i_info("Mailbox created: %s",
           str_sanitize(mailbox_get_vname(box), MAILBOX_NAME_LOG_LEN));
}

static void
virtual_keywords_mail_user_created(struct mail_user *user)
{
    // TODO create default virtual folders (All, Starred, ...)
    i_info("user created %s", user->username);

    struct virtual_keywords_user *muser;
    const char *str;

    muser = p_new(user->pool, struct virtual_keywords_user, 1);
    MODULE_CONTEXT_SET(user, virtual_keywords_user_module, muser);

    str = mail_user_plugin_getenv(user, "virtual_keywords_prefix");
    muser->prefix = str == NULL ? VIRTUAL_KEYWORDS_DEFAULT_PREFIX :
                    str;
}



static const struct notify_vfuncs virtual_keywords_vfuncs = {
    .mail_update_keywords = virtual_keywords_mail_update_keywords,
    .mailbox_create = virtual_keywords_mailbox_create,
};

static struct notify_context *virtual_keywords_ctx;

static struct mail_storage_hooks virtual_keywords_mail_storage_hooks = {
    .mail_user_created = virtual_keywords_mail_user_created
};

void
virtual_keywords_plugin_init(struct module *module)
{
    virtual_keywords_ctx = notify_register(&virtual_keywords_vfuncs);
    i_info("notify registered at %p", virtual_keywords_ctx);
    mail_storage_hooks_add(module, &virtual_keywords_mail_storage_hooks);
}

void
virtual_keywords_plugin_deinit(void)
{
    mail_storage_hooks_remove(&virtual_keywords_mail_storage_hooks);
    notify_unregister(virtual_keywords_ctx);
}

const char *virtual_keywords_plugin_dependencies[] = { "notify", "virtual", NULL };
const char virtual_keywords_plugin_binary_dependency[] = "imap";

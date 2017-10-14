
#include <dovecot/config.h>
#include <dovecot/lib.h>
#include <dovecot/str-sanitize.h>
#include <dovecot/failures.h>
#include <dovecot/ostream.h>
#include <dovecot/notify-plugin.h>
#include <dovecot/mail-storage-private.h>

#include "virtual-keywords-plugin.h"

#define MAILBOX_NAME_LOG_LEN 64
#define VIRTUAL_KEYWORDS_DEFAULT_PREFIX "Virtual."

#define VIRTUAL_KEYWORDS_MAILBOX_ALL        "All"
#define VIRTUAL_KEYWORDS_MAILBOX_STARRED    "Starred"

#define VIRTUAL_KEYWORDS_PATTERN_ALL    "*\n-Trash\n-Trash/*"
#define VIRTUAL_KEYWORDS_SEARCH_ALL     "ALL"
#define VIRTUAL_KEYWORDS_SEARCH_STARRED "FLAGGED"

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
virtual_keywords_create_mailbox(struct mail_user *user, const char *name, const char *rules)
{
    struct virtual_keywords_user *muser =
            VIRTUAL_KEYWORDS_USER_CONTEXT(user);

    struct mail_namespace *ns = mail_namespace_find_prefix(user->namespaces, muser->prefix);
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
virtual_keywords_create_keyword_rule(const char *prefix, const char *name)
{
    return t_strconcat(prefix, VIRTUAL_KEYWORDS_MAILBOX_ALL, "\n  ", VIRTUAL_KEYWORDS_SEARCH_PREFIX, name, "\n", NULL);
}

static const char *
virtual_keywords_create_child_rule(const char *prefix, const char *search)
{
    return t_strconcat(prefix, VIRTUAL_KEYWORDS_MAILBOX_ALL, "\n  ", search, "\n", NULL);
}

static const char *
virtual_keywords_create_rule(const char *patterns, const char *search)
{
    return t_strconcat(patterns, "\n  ", search, "\n", NULL);
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
        virtual_keywords_create_mailbox(mail->box->storage->user, keywords[i],
            virtual_keywords_create_keyword_rule(muser->prefix, keywords[i]));
    }
}

static void
virtual_keywords_mail_copy(void *txn, struct mail *src, struct mail *dst)
{
    i_info("######### Copying message");

    struct mail *real = NULL;
    mail_get_backend_mail(dst, &real);
    if (real != NULL) {
        i_debug("###### Got real mail at %p", real);

        struct mail_keywords *keywords = NULL;
        const char *kw[] = { "Bella", NULL };
        mailbox_keywords_create(real->box, kw, &keywords);
        i_debug("############ Keywords created at %p", keywords);
        if (keywords != NULL) {
            mail_update_keywords(real, MODIFY_ADD, keywords);
            i_info("#### Keywords updated");
            mailbox_keywords_unref(&keywords);
        }
    }
}

static void
virtual_keywords_mail_namespaces_created(struct mail_namespace *namespaces)
{
    struct virtual_keywords_user *muser =
            VIRTUAL_KEYWORDS_USER_CONTEXT(namespaces->user);

    // create default virtual folders (All, Starred, ...)
    i_info("Namespaces created for %s",
           str_sanitize(namespaces->user->username, MAILBOX_NAME_LOG_LEN));

    // All messages except Trash
    virtual_keywords_create_mailbox(namespaces->user, VIRTUAL_KEYWORDS_MAILBOX_ALL,
        virtual_keywords_create_rule(VIRTUAL_KEYWORDS_PATTERN_ALL, VIRTUAL_KEYWORDS_SEARCH_ALL));

    // All flagged messages
    virtual_keywords_create_mailbox(namespaces->user, VIRTUAL_KEYWORDS_MAILBOX_STARRED,
        virtual_keywords_create_child_rule(muser->prefix, VIRTUAL_KEYWORDS_SEARCH_STARRED));
}

static void
virtual_keywords_mail_user_created(struct mail_user *user)
{
    i_info("User created %s", user->username);

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
    .mail_copy = virtual_keywords_mail_copy,
};

static struct notify_context *virtual_keywords_ctx;

static struct mail_storage_hooks virtual_keywords_mail_storage_hooks = {
    .mail_user_created = virtual_keywords_mail_user_created,
    .mail_namespaces_created = virtual_keywords_mail_namespaces_created,
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

const char *virtual_keywords_plugin_dependencies[] = { "notify", "virtual", NULL };
const char virtual_keywords_plugin_binary_dependency[] = "imap";

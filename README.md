Dovecot Virtual Keywords Plugin
===============================

> This plugin is a work in progress.

This plugin will automatically create a virtual mailbox whenever a new keyword
is set on a message. It will also create the dovecot-virtual file needed to
map the new keyword onto a virtual mailbox:

```
Virtual.All
  keyword Important
```

On mail user creation, it will also create the following default mailboxes:

* All (all messages expect Trash)

    ```
    *
    -Trash
    -Trash/*
      all
    ```

* Starred (all flagged messages)

    ```
    Virtual.All
      starred
    ```

## Dovecot configuration

In order for this plugin to work, you need to configure a new namespace in
Dovecot for hosting virtual mailboxes.

```
mail_plugins = $mail_plugins virtual

namespace {
  type = private
  prefix = Virtual.
  separator = .
  location = virtual:~/Maildir/virtual
  list = yes
}
```

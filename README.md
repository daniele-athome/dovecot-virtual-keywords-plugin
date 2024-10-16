Dovecot Virtual Keywords Plugin
===============================

> NOTE: This project is no longer maintained.

This plugin will automatically create a [virtual mailbox](https://wiki2.dovecot.org/Plugins/Virtual)
whenever a new keyword is set on a message. It will also create the dovecot-virtual
file needed to map the new keyword onto a virtual mailbox:

```
Virtual.All
  keyword Important
```

On user login, it will also create the following default mailboxes if necessary:

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
      FLAGGED
    ```

## Install

We use CMake. Make sure to install Dovecot headers before building.

```
mkdir build
cd build
cmake ..
make
```

Then copy libvirtual_keywords_plugin.so into Dovecot modules directory, usually `/usr/lib/dovecot/modules`.

## Dovecot configuration

In order for this plugin to work, you need to configure a new namespace in
Dovecot for hosting virtual mailboxes.

```
mail_plugins = $mail_plugins virtual notify virtual_keywords

namespace {
  type = private
  prefix = Virtual.
  separator = .
  location = virtual:~/Maildir/virtual
  list = yes
}
```

The plugin accepts the following parameters:

```
plugin {
  ...

  # namespace prefix where virtual storage is configured
  virtual_keywords_prefix = "Virtual."

  # excluded keywords (no default)
  virtual_keywords_exclude = "$MDNSent,\NonJunk,\Junk,\Recent,$Forwarded,\Answered,\Flagged,\Deleted,\Seen,\Draft"
}
```

## TODO

* When a keyword is set on a message, set the same keyword to all messages in that thread
* Delete a virtual mailbox if there are no more messages with that keyword

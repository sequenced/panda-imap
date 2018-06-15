AC_ARG_WITH(mail-rel,
            AS_HELP_STRING([--with-mail-rel=PATH],
                           [set mailbox relative path (default: none)]),
            [with_mail_rel_path="$withval"],
            [with_mail_rel_path=nil])
AS_IF([test "x$with_mail_rel_path" != xnil],
      [AC_DEFINE_UNQUOTED([MAILBOX_RELATIVE_PATH],
                          ["$with_mail_rel_path"],
                          [Define to mailbox relative path])
      ],
      [AC_DEFINE([MAILBOX_RELATIVE_PATH],
                 [NIL],
                 [Define to mailbox relative path])
      ])
AC_ARG_WITH(mail-spool,
            AS_HELP_STRING([--with-mail-spool=PATH],
                           [set mail spool path (default: /var/spool/mail)]),
            [with_mail_spool_path="$withval"],
            [with_mail_spool_path=/var/spool/mail])
AC_DEFINE_UNQUOTED([MAIL_SPOOL_PATH],
                   ["$with_mail_spool_path"],
                   [Define to mail spool path])

AC_ARG_WITH(cram-md5,
            AS_HELP_STRING([--with-cram-md5=FILE],
                           [set CRAM MD5 shared secret (default: /etc/cram-md5.pwd)]),
            [with_cram_md5_file="$withval"],
            [with_cram_md5_file=/etc/cram-md5.pwd])
AC_DEFINE_UNQUOTED([MAIL_CRAM_MD5_FILE],
                   ["$with_cram_md5_file"],
                   [Define to CRAM MD5 shared secret])

AC_ARG_WITH(anon-home,
            AS_HELP_STRING([--with-anon-home=PATH],
                           [set anonymous home (default: /var/spool/mail/anonymous)]),
            [with_anon_home_path="$withval"],
            [with_anon_home_path=/var/spool/mail/anonymous])
AC_DEFINE_UNQUOTED([MAIL_ANONYMOUS_HOME_PATH],
                   ["$with_anon_home_path"],
                   [Define to anonymous home])

AC_ARG_WITH(active-file,
            AS_HELP_STRING([--with-active-file=FILE],
                           [set active file (default: /var/lib/news/active)]),
            [with_active_file="$withval"],
            [with_active_file=/var/lib/news/active])
AC_DEFINE_UNQUOTED([MAIL_ACTIVE_FILE],
                   ["$with_active_file"],
                   [Define to active file])

AC_ARG_WITH(news-spool,
            AS_HELP_STRING([--with-news-spool=PATH],
                           [set news spool path (default: /var/spool/news)]),
            [with_news_spool_path="$withval"],
            [with_news_spool_path=/var/spool/news])
AC_DEFINE_UNQUOTED([MAIL_NEWS_SPOOL_PATH],
                   ["$with_news_spool_path"],
                   [Define to news spool path])

AC_ARG_WITH(nologin,
            AS_HELP_STRING([--with-nologin=FILE],
                           [set nologin file (default: /etc/nologin)]),
            [with_nologin_file="$withval"],
            [with_nologin_file=/etc/nologin])
AC_DEFINE_UNQUOTED([MAIL_NOLOGIN_FILE],
                   ["$with_nologin_file"],
                   [Define to nologin file])

.. highlightlang:: none

Debian GNU/Linux
================

This section describes how to install Mroonga related deb packages on
Debian GNU/Linux. You can install them by ``apt``.

.. include:: 32bit-note.inc

jessie (MySQL)
--------------

/etc/apt/sources.list.d/groonga.list::

  deb https://packages.groonga.org/debian/ jessie main
  deb-src https://packages.groonga.org/debian/ jessie main

Install::

  % sudo apt-get install apt-transport-https
  % sudo apt-get update
  % sudo apt-get install -y --allow-unauthenticated groonga-keyring
  % sudo apt-get update
  % sudo apt-get install -y -V mysql-server-mroonga

If you want to use `MeCab <http://mecab.sourceforge.net/>`_ as a tokenizer, install groonga-tokenizer-mecab package.

Install groonga-tokenizer-mecab package::

  % sudo apt-get install -y -V groonga-tokenizer-mecab

jessie (MariaDB)
----------------

/etc/apt/sources.list.d/groonga.list::

  deb https://packages.groonga.org/debian/ jessie main
  deb-src https://packages.groonga.org/debian/ jessie main

Install::

  % sudo apt-get install apt-transport-https
  % sudo apt-get update
  % sudo apt-get install -y --allow-unauthenticated groonga-keyring
  % sudo apt-get update
  % sudo apt-get install -y -V mariadb-server-10.0-mroonga

If you want to use `MeCab <http://mecab.sourceforge.net/>`_ as a tokenizer, install groonga-tokenizer-mecab package.

Install groonga-tokenizer-mecab package::

  % sudo apt-get install -y -V groonga-tokenizer-mecab


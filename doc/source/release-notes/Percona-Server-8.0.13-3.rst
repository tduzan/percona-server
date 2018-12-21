.. rn:: 8.0.13-3

================================================================================
|Percona Server| |release|
================================================================================

|Percona| announces the GA release of |Percona Server| |release| on
|date| (downloads are available `here <https://www.percona.com/downloads/Percona-Server-8.0/>`__ and from the `Percona SoftwareRepositories <https://www.percona.com/doc/percona-server/8.0/installation.html#installing-from-binaries>`__). 
This release merges changes of |MySQL| 8.0.13, including
all the bug fixes in it. |Percona Server| |release| is now the
current GA release in the 8.0 series. All of |Percona|’s software is
open-source and free.

Features in |Percona Server| 8.0
================================================================================

Percona Server for MySQL 8.0 includes all the `features available in
MySQL 8.0 Community
Edition <https://dev.mysql.com/doc/refman/8.0/en/mysql-nutshell.html>`__
in addition to enterprise-grade features developed by Percona for the
community.

MySQL 8.0 Features
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In addition to other features, some of the highlights from MySQL 8.0
that are present in Percona Server for MySQL 8.0 include:

-  Transactional Data Dictionary which enables crash-safe and atomic DDL
   operations
-  SQL Roles to make implementing role-based access controls simplified
-  Resource Groups which allow you to `fine-tune resource
   management <https://www.percona.com/blog/2018/08/27/mysql-8-load-fine-tuning-with-resource-groups/>`__
-  Instant ``ADD COLUMN`` by specifying ``ALGORITHM=INSTANT`` on
   supported DDL operations
-  JSON enhancements including new operators and functions for working
   with JSON data and data types in MySQL.
-  Addition of Common Table Expressions and Window Functions
-  Unicode safe regular expressions through ICU support.
-  Support for spatial data types and indexes, including SRS functions.

Please see the `upstream documentation <https://dev.mysql.com/doc/refman/8.0/en/>`__ for more details.

Percona Server for MySQL 8.0 Features
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Building on upstream MySQL 8.0 Community Edition, Percona Server for
MySQL 8.0 brings many great features to the community in this release,
including the following:

-  Parallel doublewrite buffer, split mutexes, multi-threaded LRU
   flushers, and other InnoDB performance improvements as part of XtraDB
-  Audit Logging and PAM-based authentication plugins to bring
   enterprise security and compliance features to the community.
-  Threadpooling
-  Enhanced encryption functionality that builds on top of the
   transparent data encryption (TDE) that’s present in MySQL Community
   Edition by adding support for binary log encryption, temporary file
   encryption, encryption support for all InnoDB tablespace types and
   logs, encryption of the parallel doublewrite buffer, key rotation,
   and support for centralized key management using Hashicorp Vault.
   Please Note: Many of the encryption features are still considered
   alpha or beta quality and may not yet be suitable for production use.
-  MyRocks Storage Engine optimized for modern hardware, utilizing
   strong compression to reduce wear and storage requirements on SSDs,
   increasing ROI and lowering TCO for large data sets. Now with Native
   Partitioning support in Percona Server for MySQL 8.0
-  TokuDB Storage Engine based on PerconaFT optimized for write-heavy
   workloads, now with Native Partitioning support in Percona Server for
   MySQL 8.0. Please Note: TokuDB is deprecated in Percona Server for
   MySQL 8.0 and will be supported throughout this release series, but
   will not be included in 9.0.
-  Significant improvements to instrumentation, including more than
   double the available performance and status counters, support for
   user statistics and thread statistics, and extended slow query
   logging.
-  InnoDB Column Compression with Data Dictionary support and Per-Column
   compression for ``VARCHAR``/``BLOB`` and ``JSON`` data types.
-  InnoDB Changed Page Tracking and Extended ``SHOW GRANTS`` to make
   auditing and compliance easier.
-  Improved backup locking to reduce performance and availability impact
   of performing physical backups using Percona XtraBackup 8.0.

Features Removed in Percona Server for MySQL 8.0
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

-  Slow Query Log Rotation and Expiration
-  CSV engine mode for standard-compliant quote and comma parsing
-  Utility user
-  Expanded program option modifiers
-  The ``ALL_O_DIRECT`` InnoDB flush method: it is not compatible with the
   new redo logging implementation
-  ``XTRADB_RSEG`` table from ``INFORMATION_SCHEMA``
-  InnoDB memory size information from ``SHOW ENGINE INNODB STATUS;`` the
   same information is available from Performance Schema memory summary
   tables
-  Query cache enhancements: The query cache is no longer present in
   MySQL 8.0

Improvements
================================================================================

-  :psbug:`5153`: Ensured that session temporary tables are covered by Temporary
   Tablespace Encryption feature 
-  :psbug:`5151`: Changed the way that General Tablespace Encryption feature registers
   with the data dictionary to enable clean upgrades from PS 5.7 to PS
   8.0.13
-  :psbug:`5105`: Rewrite Binlog Encryption after MySQL 8.0.13 merge due to WL#10956
   that changed binlog access APIs
-  :psbug:`5195`: Disabled the variables for Expanded Fast Index Creation since the
   feature is not yet re-implemented
-  :psbug:`5014`: Update Percona Backup Locks feature to use the new ``BACKUP_ADMIN``
   privilege in MySQL 8.0
-  :psbug:`4805`: Re-Implemented Compressed Columns with Dictionaries feature in PS 8.0
-  :psbug:`4790`: Improved accuracy of User Statistics feature

Bugs Fixed Since 8.0.12-rc1
================================================================================

-  Fixed a bug in Temporary Tablespace Encryption feature in which a
   temporary table could be encrypted and written to disk but read back
   as if it were not encrypted. :psbug:`5180`
-  Fixed a crash bug on some simple SQL queries in TokuDB :psbug:`5163`
-  Fixed a buffer overflow in TokuDB when a database is created at
   maximum length containing filesystem unfriendly characters :psbug:`5158`
-  Fixed a memory leak in the ``binlog_event_deserialize`` function used
   by Binlog Encryption feature :psbug:`5147`
-  Fixed a memory leak in ``mysqldump`` in the ``--innodb-optimize-keys`` 
   functionality :psbug:`5144`
-  Fixed a crash in ``mysqldump`` in the ``--innodb-optimize-keys``
   functionality :psbug:`4972`
-  Fixed a crash that can occur when system tables are locked by the
   user due to a ``lock_wait_timeout`` :psbug:`5134`
-  Fixed a crash that can occur when system tables are locked by the
   user from a ``SELECT FOR UPDATE`` statement :psbug:`5027`
-  Fixed a bug that would prevent upgrading from PS 5.7 to PS 8.0 if you
   had bootstrapped your datadir with ``--innodb-encrypt-tables`` :psbug:`5117`
-  Fixed a bug that caused ``innodb_buffer_pool_size`` to be
   uninitialized after a restart if it was set using ``SET PERSIST`` :psbug:`5069`
-  Fixed a crash in TokuDB that can occur when a temporary table
   experiences an autoincrement rollover :psbug:`5056`
-  Fixed a bug where marking an index as invisible would cause a table
   rebuild in TokuDB and also in MyRocks :psbug:`5031`
-  Fixed a crash under some conditions when using the ``VARBINARY`` data
   type in a table. :psbug:`5025`
-  Fixed a crash that would occur when querying PFS metadata locks table
   after ``FLUSH TABLE WITH READ LOCK`` :psbug:`4977`
-  Fixed a bug where audit logs could get corrupted if the
   ``audit_log_rotations`` was changed during runtime. :psbug:`4950`
-  Fixed a bug where ``LOCK INSTANCE FOR BACKUP`` and
   ``STOP SLAVE SQL_THREAD`` would cause replication to be blocked and
   unable to be restarted. :psbug:`4758` (Upstream :mysqlbug:`93649`)

Other Bugs Fixed:

:psbug:`5155`, :psbug:`5140`, :psbug:`5139`, :psbug:`5120`, :psbug:`5108`, :psbug:`5091`,
:psbug:`5057`, :psbug:`5049`, :psbug:`5041`, :psbug:`5016`, :psbug:`4999`, :psbug:`4971`,
:psbug:`4943`, :psbug:`4926`, :psbug:`4920`, :psbug:`4918`, :psbug:`4917`, :psbug:`4898`,
:psbug:`4796`, and :psbug:`4744`.

Known Issues
================================================================================

Due to the significant structural changes in 8.0, this was a more
difficult porting process than is typical. We have a few features and
issues outstanding that should be resolved in the next release.

Pending Feature Re-Implementations and Improvements
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

-  `PS-4892 <https://jira.percona.com/browse/PS-4892>`__: Re-Implement
   Expanded Fast Index Creation feature.
-  `PS-5216 <https://jira.percona.com/browse/PS-5216>`__: Re-Implement Utility User feature.
-  `PS-5143 <https://jira.percona.com/browse/PS-5143>`__: Identify
   Percona features which can make use of dynamic privileges instead of
   SUPER

Notable Issues in Features
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

-  `PS-5148 <https://jira.percona.com/browse/PS-5148>`__: Regression in
   Compressed Columns Feature when using ``innodb-force-recovery``
-  `PS-4996 <https://jira.percona.com/browse/PS-4996>`__: Regression in
   User Statistics feature where ``TOTAL_CONNECTIONS`` field reports
   incorrect data
-  `PS-4933 <https://jira.percona.com/browse/PS-4933>`__: Regression in
   Slow Query Logging Extensions feature where incorrect transaction id
   accounting can cause an assert during certain DDLs.
-  `PS-4748 <https://jira.percona.com/browse/PS-4748>`__: TokuDB: Toku
   HotBackup (experimental feature) is currently not fully functional,
   the correct fix is related to an upcoming upstream bugfix.
-  `PS-5206 <https://jira.percona.com/browse/PS-5206>`__: TokuDB: A
   crash can occur in TokuDB when using Native Partioning and the
   optimizer has ``index_merge_union`` enabled. Workaround by using
   ``SET SESSION optimizer_switch="index_merge_union=off";``
-  `PS-5174 <https://jira.percona.com/browse/PS-5174>`__: MyRocks:
   Attempting to use unsupported features against MyRocks can lead to a
   crash rather than an error.
-  `PS-5024 <https://jira.percona.com/browse/PS-5024>`__: MyRocks:
   Queries can return the wrong results on tables with no primary key,
   non-unique CHAR/VARCHAR rows, and UTF8MB4 charset.
-  `PS-5045 <https://jira.percona.com/browse/PS-5045>`__: MyRocks:
   Altering a column or table comment cause the table to be rebuilt

Find the release notes for Percona Server for MySQL 8.0.13-3 in our online documentation. Report bugs in the Jira bug tracker.

.. |release| replace:: 8.0.13-3
.. |date| replace:: December 21, 2018
		       

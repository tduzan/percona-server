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

Percona Server for MySQL 8.0 includes all the `features available in
MySQL 8.0 Community
Edition <https://dev.mysql.com/doc/refman/8.0/en/mysql-nutshell.html>`__
in addition to enterprise-grade features developed by Percona for the
community.  For a list of highlighted features from both MySQL 8.0 and 
Percona Server for MySQL 8.0, please see the `GA release announcement <https://www.percona.com/blog/2018/12/21/announcing-general-availability-of-percona-server-for-mysql-8-0/>`__.


Features Removed in Percona Server for MySQL 8.0
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

-  Slow Query Log Rotation and Expiration
-  CSV engine mode for standard-compliant quote and comma parsing
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

-  :psbug:`5014`: Update Percona Backup Locks feature to use the new ``BACKUP_ADMIN``
   privilege in MySQL 8.0
-  :psbug:`4805`: Re-Implemented Compressed Columns with Dictionaries feature in PS 8.0
-  :psbug:`4790`: Improved accuracy of User Statistics feature

Bugs Fixed Since 8.0.12-rc1
================================================================================

-  Ensured that session temporary tables are covered by Temporary
   Tablespace Encryption feature :psbug:`5153`
-  Changed the way that General Tablespace Encryption feature registers
   with the data dictionary to enable clean upgrades from PS 5.7 to PS
   8.0.13 :psbug:`5151`
-  Rewrite Binlog Encryption after MySQL 8.0.13 merge due to `WL#10956 <https://dev.mysql.com/worklog/task/?id=10956>`__
   that changed binlog access APIs :psbug:`5105`
-  Disabled the variables for Expanded Fast Index Creation since the
   feature is not yet re-implemented :psbug:`5195`
-  Fixed a crash bug on some simple SQL queries in TokuDB :psbug:`5163`
-  Fixed a buffer overflow in TokuDB when a database is created at
   maximum length containing filesystem unfriendly characters :psbug:`5158`
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
:psbug:`4796`, :psbug:`5147`, :psbug:`5180`, and :psbug:`4744`.

Known Issues
================================================================================

Due to the significant structural changes in 8.0, this was a more
difficult porting process than is typical. We have a few features and
issues outstanding that should be resolved in the next release.

Pending Feature Re-Implementations and Improvements
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

-  :psbug:`4892`: Re-Implement Expanded Fast Index Creation feature.
-  :psbug:`5216`: Re-Implement Utility User feature.
-  :psbug:`5143`: Identify Percona features which can make use of dynamic privileges instead of ``SUPER``

Notable Issues in Features
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

-  :psbug:`5148`: Regression in Compressed Columns Feature when using ``innodb-force-recovery``
-  :psbug:`4996`: Regression in User Statistics feature where ``TOTAL_CONNECTIONS`` field report incorrect data
-  :psbug:`4933`: Regression in Slow Query Logging Extensions feature where incorrect transaction id
   accounting can cause an assert during certain DDLs.
-  :psbug:`5206`: TokuDB: A crash can occur in TokuDB when using Native Partioning and the optimizer 
    has ``index_merge_union`` enabled. Workaround by using ``SET SESSION optimizer_switch="index_merge_union=off";``
-  :psbug:`5174`: MyRocks: Attempting to use unsupported features against MyRocks can lead to a crash rather than an error.
-  :psbug:`5024`: MyRocks: Queries can return the wrong results on tables with no primary key, non-unique
    ``CHAR``/``VARCHAR`` rows, and ``UTF8MB4`` charset.
-  :psbug:`5045`: MyRocks: Altering a column or table comment cause the table to be rebuilt

Find the release notes for Percona Server for MySQL 8.0.13-3 in our online documentation. Report bugs in the Jira bug tracker.

.. |release| replace:: 8.0.13-3
.. |date| replace:: December 21, 2018
		       

/* Copyright (c) 2000, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @addtogroup Replication
  @{

  @file
  
  @brief Binary log event definitions.  This includes generic code
  common to all types of log events, as well as specific code for each
  type of log event.
*/


#ifndef _log_event_h
#define _log_event_h

#include <my_bitmap.h>
#include "rpl_constants.h"
/* These two header files are necessary for the List manipuation */
#include "sql_list.h"                           /* I_List */
#include "hash.h"
#include "table_id.h"

#ifdef MYSQL_CLIENT
#include "sql_const.h"
#include "rpl_utility.h"
#include "hash.h"
#include "rpl_tblmap.h"

/*
  Variable to suppress the USE <DATABASE> command when using the
  new mysqlbinlog option
*/
bool option_rewrite_set= FALSE;
extern I_List<i_string_pair> binlog_rewrite_db;
#endif

#ifdef MYSQL_SERVER
#include "rpl_record.h"
#include "rpl_reporting.h"
#include "sql_class.h"                          /* THD */
#include "rpl_utility.h"                        /* Hash_slave_rows */
#include "rpl_filter.h"
#endif

#include "binary_log.h"
using  binary_log::Binary_log_event;
/* Forward declarations */
class String;
typedef ulonglong sql_mode_t;
typedef struct st_db_worker_hash_entry db_worker_hash_entry;

#define PREFIX_SQL_LOAD "SQL_LOAD-"

/**
   Maximum length of the name of a temporary file
   PREFIX LENGTH - 9 
   UUID          - UUID_LENGTH
   SEPARATORS    - 2
   SERVER ID     - 10 (range of server ID 1 to (2^32)-1 = 4,294,967,295)
   FILE ID       - 10 (uint)
   EXTENSION     - 7  (Assuming that the extension is always less than 7 
                       characters)
*/
#define TEMP_FILE_MAX_LEN UUID_LENGTH+38 

/**
   Either assert or return an error.

   In debug build, the condition will be checked, but in non-debug
   builds, the error code given will be returned instead.

   @param COND   Condition to check
   @param ERRNO  Error number to return in non-debug builds
*/
#ifdef DBUG_OFF
#define ASSERT_OR_RETURN_ERROR(COND, ERRNO) \
  do { if (!(COND)) return ERRNO; } while (0)
#else
#define ASSERT_OR_RETURN_ERROR(COND, ERRNO) \
  DBUG_ASSERT(COND)
#endif

#define LOG_READ_EOF    -1
#define LOG_READ_BOGUS  -2
#define LOG_READ_IO     -3
#define LOG_READ_MEM    -5
#define LOG_READ_TRUNC  -6
#define LOG_READ_TOO_LARGE -7
#define LOG_READ_CHECKSUM_FAILURE -8

#define LOG_EVENT_OFFSET 4

/*
   3 is MySQL 4.x; 4 is MySQL 5.0.0.
   Compared to version 3, version 4 has:
   - a different Start_log_event, which includes info about the binary log
   (sizes of headers); this info is included for better compatibility if the
   master's MySQL version is different from the slave's.
   - all events have a unique ID (the triplet (server_id, timestamp at server
   start, other) to be sure an event is not executed more than once in a
   multimaster setup, example:
                M1
              /   \
             v     v
             M2    M3
             \     /
              v   v
                S
   if a query is run on M1, it will arrive twice on S, so we need that S
   remembers the last unique ID it has processed, to compare and know if the
   event should be skipped or not. Example of ID: we already have the server id
   (4 bytes), plus:
   timestamp_when_the_master_started (4 bytes), a counter (a sequence number
   which increments every time we write an event to the binlog) (3 bytes).
   Q: how do we handle when the counter is overflowed and restarts from 0 ?

   - Query and Load (Create or Execute) events may have a more precise
     timestamp (with microseconds), number of matched/affected/warnings rows
   and fields of session variables: SQL_MODE,
   FOREIGN_KEY_CHECKS, UNIQUE_CHECKS, SQL_AUTO_IS_NULL, the collations and
   charsets, the PASSWORD() version (old/new/...).
*/
#define BINLOG_VERSION    4

#define NUM_LOAD_DELIM_STRS 5

/*****************************************************************************
  sql_ex_info struct
  The strcture contains a refernce to another structure sql_ex_data_info,
  which is defined in binlogapi, and contains the characters specified in
  the sub clause of a LOAD_DATA_INFILE.
  //TODO: Remove this struct and only retain binary_log::sql_ex_data_info
          when the encoder is moved to bapi

 ****************************************************************************/
struct sql_ex_info
{
  sql_ex_info() {}                            /* Remove gcc warning */
  binary_log::sql_ex_data_info data_info;

  bool write_data(IO_CACHE* file);
  const char* init(const char* buf, const char* buf_end, bool use_new_format);
};

/*****************************************************************************

  MySQL Binary Log

  This log consists of events.  Each event has a fixed-length header,
  possibly followed by a variable length data body.

  The data body consists of an optional fixed length segment (post-header)
  and  an optional variable length segment.

  See the #defines below for the format specifics.

  The events which really update data are Query_log_event,
  Execute_load_query_log_event and old Load_log_event and
  Execute_load_log_event events (Execute_load_query is used together with
  Begin_load_query and Append_block events to replicate LOAD DATA INFILE.
  Create_file/Append_block/Execute_load (which includes Load_log_event)
  were used to replicate LOAD DATA before the 5.0.3).

 ****************************************************************************/

#define MAX_LOG_EVENT_HEADER   ( /* in order of Query_log_event::write */ \
  (LOG_EVENT_HEADER_LEN + /* write_header */ \
  Binary_log_event::QUERY_HEADER_LEN     + /* write_data */   \
  Binary_log_event::EXECUTE_LOAD_QUERY_EXTRA_HEADER_LEN) + /*write_post_header_for_derived */ \
  MAX_SIZE_LOG_EVENT_STATUS + /* status */ \
  NAME_LEN + 1)

/*
  The new option is added to handle large packets that are sent from the master 
  to the slave. It is used to increase the thd(max_allowed) for both the
  DUMP thread on the master and the SQL/IO thread on the slave. 
*/
#define MAX_MAX_ALLOWED_PACKET 1024*1024*1024


/* slave event post-header (this event is never written) */

#define SL_MASTER_PORT_OFFSET   8
#define SL_MASTER_POS_OFFSET    0
#define SL_MASTER_HOST_OFFSET   10

/*
  Q_COMMIT_TS status variable stores the logical timestamp when the transaction
  entered the commit phase. This wll be used to apply transactions in parallel
  on the slave.
#define Q_COMMIT_TS 14
 */

/*
  G_COMMIT_TS status variable stores the logical timestamp when the transaction
  entered the commit phase. This wll be used to apply transactions in parallel
  on the slave.
 */
#define G_COMMIT_TS  1

/* Intvar event post-header */

/* Intvar event data */
#define I_TYPE_OFFSET        0
#define I_VAL_OFFSET         1

/* TM = "Table Map" */
#define TM_MAPID_OFFSET    0
#define TM_FLAGS_OFFSET    6

/* RW = "RoWs" */
#define RW_MAPID_OFFSET    0
#define RW_FLAGS_OFFSET    6
#define RW_VHLEN_OFFSET    8
#define RW_V_TAG_LEN       1
#define RW_V_EXTRAINFO_TAG 0

/* 4 bytes which all binlogs should begin with */
#define BINLOG_MAGIC        "\xfe\x62\x69\x6e"

/*
  The 2 flags below were useless :
  - the first one was never set
  - the second one was set in all Rotate events on the master, but not used for
  anything useful.
  So they are now removed and their place may later be reused for other
  flags. Then one must remember that Rotate events in 4.x have
  LOG_EVENT_FORCED_ROTATE_F set, so one should not rely on the value of the
  replacing flag when reading a Rotate event.
  I keep the defines here just to remember what they were.

  #define LOG_EVENT_TIME_F            0x1
  #define LOG_EVENT_FORCED_ROTATE_F   0x2
*/

/*
   This flag only makes sense for Format_description_log_event. It is set
   when the event is written, and *reset* when a binlog file is
   closed (yes, it's the only case when MySQL modifies already written
   part of binlog).  Thus it is a reliable indicator that binlog was
   closed correctly.  (Stop_log_event is not enough, there's always a
   small chance that mysqld crashes in the middle of insert and end of
   the binlog would look like a Stop_log_event).

   This flag is used to detect a restart after a crash, and to provide
   "unbreakable" binlog. The problem is that on a crash storage engines
   rollback automatically, while binlog does not.  To solve this we use this
   flag and automatically append ROLLBACK to every non-closed binlog (append
   virtually, on reading, file itself is not changed). If this flag is found,
   mysqlbinlog simply prints "ROLLBACK" Replication master does not abort on
   binlog corruption, but takes it as EOF, and replication slave forces a
   rollback in this case.

   Note, that old binlogs does not have this flag set, so we get a
   a backward-compatible behaviour.
*/

#define LOG_EVENT_BINLOG_IN_USE_F       0x1

/**
  @def LOG_EVENT_THREAD_SPECIFIC_F

  If the query depends on the thread (for example: TEMPORARY TABLE).
  Currently this is used by mysqlbinlog to know it must print
  SET @@PSEUDO_THREAD_ID=xx; before the query (it would not hurt to print it
  for every query but this would be slow).
*/
#define LOG_EVENT_THREAD_SPECIFIC_F 0x4

/**
  @def LOG_EVENT_SUPPRESS_USE_F

  Suppress the generation of 'USE' statements before the actual
  statement. This flag should be set for any events that does not need
  the current database set to function correctly. Most notable cases
  are 'CREATE DATABASE' and 'DROP DATABASE'.

  This flags should only be used in exceptional circumstances, since
  it introduce a significant change in behaviour regarding the
  replication logic together with the flags --binlog-do-db and
  --replicated-do-db.
 */
#define LOG_EVENT_SUPPRESS_USE_F    0x8

/*
  Note: this is a place holder for the flag
  LOG_EVENT_UPDATE_TABLE_MAP_VERSION_F (0x10), which is not used any
  more, please do not reused this value for other flags.
 */

/**
   @def LOG_EVENT_ARTIFICIAL_F
   
   Artificial events are created arbitarily and not written to binary
   log

   These events should not update the master log position when slave
   SQL thread executes them.
*/
#define LOG_EVENT_ARTIFICIAL_F 0x20

/**
   @def LOG_EVENT_RELAY_LOG_F
   
   Events with this flag set are created by slave IO thread and written
   to relay log
*/
#define LOG_EVENT_RELAY_LOG_F 0x40

/**
   @def LOG_EVENT_IGNORABLE_F

   For an event, 'e', carrying a type code, that a slave,
   's', does not recognize, 's' will check 'e' for
   LOG_EVENT_IGNORABLE_F, and if the flag is set, then 'e'
   is ignored. Otherwise, 's' acknowledges that it has
   found an unknown event in the relay log.
*/
#define LOG_EVENT_IGNORABLE_F 0x80

/**
   @def LOG_EVENT_NO_FILTER_F

   Events with this flag are not filtered (e.g. on the current
   database) and are always written to the binary log regardless of
   filters.
*/
#define LOG_EVENT_NO_FILTER_F 0x100

/**
   MTS: group of events can be marked to force its execution
   in isolation from any other Workers.
   So it's a marker for Coordinator to memorize and perform necessary
   operations in order to guarantee no interference from other Workers.
   The flag can be set ON only for an event that terminates its group.
   Typically that is done for a transaction that contains 
   a query accessing more than OVER_MAX_DBS_IN_EVENT_MTS databases.
*/
#define LOG_EVENT_MTS_ISOLATE_F 0x200


/**
  @def OPTIONS_WRITTEN_TO_BIN_LOG

  OPTIONS_WRITTEN_TO_BIN_LOG are the bits of thd->options which must
  be written to the binlog. OPTIONS_WRITTEN_TO_BIN_LOG could be
  written into the Format_description_log_event, so that if later we
  don't want to replicate a variable we did replicate, or the
  contrary, it's doable. But it should not be too hard to decide once
  for all of what we replicate and what we don't, among the fixed 32
  bits of thd->options.

  I (Guilhem) have read through every option's usage, and it looks
  like OPTION_AUTO_IS_NULL and OPTION_NO_FOREIGN_KEYS are the only
  ones which alter how the query modifies the table. It's good to
  replicate OPTION_RELAXED_UNIQUE_CHECKS too because otherwise, the
  slave may insert data slower than the master, in InnoDB.
  OPTION_BIG_SELECTS is not needed (the slave thread runs with
  max_join_size=HA_POS_ERROR) and OPTION_BIG_TABLES is not needed
  either, as the manual says (because a too big in-memory temp table
  is automatically written to disk).
*/
#define OPTIONS_WRITTEN_TO_BIN_LOG \
  (OPTION_AUTO_IS_NULL | OPTION_NO_FOREIGN_KEY_CHECKS |  \
   OPTION_RELAXED_UNIQUE_CHECKS | OPTION_NOT_AUTOCOMMIT)

/* Shouldn't be defined before */
#define EXPECTED_OPTIONS \
  ((ULL(1) << 14) | (ULL(1) << 26) | (ULL(1) << 27) | (ULL(1) << 19))

#if OPTIONS_WRITTEN_TO_BIN_LOG != EXPECTED_OPTIONS
#error OPTIONS_WRITTEN_TO_BIN_LOG must NOT change their values!
#endif
#undef EXPECTED_OPTIONS         /* You shouldn't use this one */

enum Int_event_type
{
  INVALID_INT_EVENT = 0, LAST_INSERT_ID_EVENT = 1, INSERT_ID_EVENT = 2
};


#ifdef MYSQL_SERVER
class String;
class MYSQL_BIN_LOG;
class THD;
#endif

class Format_description_log_event;
class Relay_log_info;
class Slave_worker;
class Slave_committed_queue;

#ifdef MYSQL_CLIENT
enum enum_base64_output_mode {
  BASE64_OUTPUT_NEVER= 0,
  BASE64_OUTPUT_AUTO= 1,
  BASE64_OUTPUT_UNSPEC= 2,
  BASE64_OUTPUT_DECODE_ROWS= 3,
  /* insert new output modes here */
  BASE64_OUTPUT_MODE_COUNT
};

/*
  A structure for mysqlbinlog to know how to print events

  This structure is passed to the event's print() methods,

  There are two types of settings stored here:
  1. Last db, flags2, sql_mode etc comes from the last printed event.
     They are stored so that only the necessary USE and SET commands
     are printed.
  2. Other information on how to print the events, e.g. short_form,
     hexdump_from.  These are not dependent on the last event.
*/
typedef struct st_print_event_info
{
  /*
    Settings for database, sql_mode etc that comes from the last event
    that was printed.  We cache these so that we don't have to print
    them if they are unchanged.
  */
  // TODO: have the last catalog here ??
  char db[FN_REFLEN+1]; // TODO: make this a LEX_STRING when thd->db is
  bool flags2_inited;
  uint32 flags2;
  bool sql_mode_inited;
  sql_mode_t sql_mode;		/* must be same as THD.variables.sql_mode */
  ulong auto_increment_increment, auto_increment_offset;
  bool charset_inited;
  char charset[6]; // 3 variables, each of them storable in 2 bytes
  char time_zone_str[MAX_TIME_ZONE_NAME_LENGTH];
  uint lc_time_names_number;
  uint charset_database_number;
  uint thread_id;
  bool thread_id_printed;

  st_print_event_info();

  ~st_print_event_info() {
    close_cached_file(&head_cache);
    close_cached_file(&body_cache);
    close_cached_file(&footer_cache);
  }
  bool init_ok() /* tells if construction was successful */
    { return my_b_inited(&head_cache) && 
	     my_b_inited(&body_cache) && 
  	     my_b_inited(&footer_cache); }


  /* Settings on how to print the events */
  bool short_form;
  enum_base64_output_mode base64_output_mode;
  /*
    This is set whenever a Format_description_event is printed.
    Later, when an event is printed in base64, this flag is tested: if
    no Format_description_event has been seen, it is unsafe to print
    the base64 event, so an error message is generated.
  */
  bool printed_fd_event;
  my_off_t hexdump_from;
  uint8 common_header_len;
  char delimiter[16];

  uint verbose;
  table_mapping m_table_map;
  table_mapping m_table_map_ignored;

  /*
     These three caches are used by the row-based replication events to
     collect the header information and the main body of the events
     making up a statement and in footer section any verbose related details 
     or comments related to the statment.
   */
  IO_CACHE head_cache;
  IO_CACHE body_cache;
  IO_CACHE footer_cache; 
  /* Indicate if the body cache has unflushed events */
  bool have_unflushed_events;

  /*
     True if an event was skipped while printing the events of
     a transaction and no COMMIT statement or XID event was ever
     output (ie, was filtered out as well). This can be triggered
     by the --database option of mysqlbinlog.

     False, otherwise.
   */
  bool skipped_event_in_transaction;
} PRINT_EVENT_INFO;
#endif

/**
  @class Log_event

  This is the abstract base class for binary log events.
  
  @section Log_event_binary_format Binary Format

  Any @c Log_event saved on disk consists of the following three
  components.

  - Common-Header
  - Post-Header
  - Body

  The Common-Header, documented in the table @ref Table_common_header
  "below", always has the same form and length within one version of
  MySQL.  Each event type specifies a format and length of the
  Post-Header.  The length of the Common-Header is the same for all
  events of the same type.  The Body may be of different format and
  length even for different events of the same type.  The binary
  formats of Post-Header and Body are documented separately in each
  subclass.  The binary format of Common-Header is as follows.

  <table>
  <caption>Common-Header</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>timestamp</td>
    <td>4 byte unsigned integer</td>
    <td>The time when the query started, in seconds since 1970.
    </td>
  </tr>

  <tr>
    <td>type</td>
    <td>1 byte enumeration</td>
    <td>See enum #Log_event_type.</td>
  </tr>

  <tr>
    <td>server_id</td>
    <td>4 byte unsigned integer</td>
    <td>Server ID of the server that created the event.</td>
  </tr>

  <tr>
    <td>total_size</td>
    <td>4 byte unsigned integer</td>
    <td>The total size of this event, in bytes.  In other words, this
    is the sum of the sizes of Common-Header, Post-Header, and Body.
    </td>
  </tr>

  <tr>
    <td>master_position</td>
    <td>4 byte unsigned integer</td>
    <td>The position of the next event in the master binary log, in
    bytes from the beginning of the file.  In a binlog that is not a
    relay log, this is just the position of the next event, in bytes
    from the beginning of the file.  In a relay log, this is
    the position of the next event in the master's binlog.
    </td>
  </tr>

  <tr>
    <td>flags</td>
    <td>2 byte bitfield</td>
    <td>See Log_event::flags.</td>
  </tr>
  </table>

  Summing up the numbers above, we see that the total size of the
  common header is 19 bytes.

  @subsection Log_event_format_of_atomic_primitives Format of Atomic Primitives

  - All numbers, whether they are 16-, 24-, 32-, or 64-bit numbers,
  are stored in little endian, i.e., the least significant byte first,
  unless otherwise specified.

  @anchor packed_integer
  - Some events use a special format for efficient representation of
  unsigned integers, called Packed Integer.  A Packed Integer has the
  capacity of storing up to 8-byte integers, while small integers
  still can use 1, 3, or 4 bytes.  The value of the first byte
  determines how to read the number, according to the following table:

  <table>
  <caption>Format of Packed Integer</caption>

  <tr>
    <th>First byte</th>
    <th>Format</th>
  </tr>

  <tr>
    <td>0-250</td>
    <td>The first byte is the number (in the range 0-250), and no more
    bytes are used.</td>
  </tr>

  <tr>
    <td>252</td>
    <td>Two more bytes are used.  The number is in the range
    251-0xffff.</td>
  </tr>

  <tr>
    <td>253</td>
    <td>Three more bytes are used.  The number is in the range
    0xffff-0xffffff.</td>
  </tr>

  <tr>
    <td>254</td>
    <td>Eight more bytes are used.  The number is in the range
    0xffffff-0xffffffffffffffff.</td>
  </tr>

  </table>

  - Strings are stored in various formats.  The format of each string
  is documented separately.
*/

//TODO the dependency on Binary_log_event will be removed in future
class Log_event : public virtual Binary_log_event
{
public:
  /**
     Enumeration of what kinds of skipping (and non-skipping) that can
     occur when the slave executes an event.

     @see shall_skip
     @see do_shall_skip
   */
  enum enum_skip_reason {
    /**
       Don't skip event.
    */
    EVENT_SKIP_NOT,

    /**
       Skip event by ignoring it.

       This means that the slave skip counter will not be changed.
    */
    EVENT_SKIP_IGNORE,

    /**
       Skip event and decrease skip counter.
    */
    EVENT_SKIP_COUNT
  };

protected:
  enum enum_event_cache_type 
  {
    EVENT_INVALID_CACHE= 0,
    /* 
      If possible the event should use a non-transactional cache before
      being flushed to the binary log. This means that it must be flushed
      right after its correspondent statement is completed.
    */
    EVENT_STMT_CACHE,
    /* 
      The event should use a transactional cache before being flushed to
      the binary log. This means that it must be flushed upon commit or 
      rollback. 
    */
    EVENT_TRANSACTIONAL_CACHE,
    /* 
      The event must be written directly to the binary log without going
      through any cache.
    */
    EVENT_NO_CACHE,
    /*
       If there is a need for different types, introduce them before this.
    */
    EVENT_CACHE_COUNT
  };

  enum enum_event_logging_type
  {
    EVENT_INVALID_LOGGING= 0,
    /*
      The event must be written to a cache and upon commit or rollback
      written to the binary log.
    */
    EVENT_NORMAL_LOGGING,
    /*
      The event must be written to an empty cache and immediatly written
      to the binary log without waiting for any other event.
    */
    EVENT_IMMEDIATE_LOGGING,
    /*
       If there is a need for different types, introduce them before this.
    */
    EVENT_CACHE_LOGGING_COUNT
  };

public:
  /*
     A temp buffer for read_log_event; it is later analysed according to the
     event's type, and its content is distributed in the event-specific fields.
  */
  char *temp_buf;
  /* The number of seconds the query took to run on the master. */
  ulong exec_time;

  /*
    The master's server id (is preserved in the relay log; used to
    prevent from infinite loops in circular replication).
  */
  uint32 server_id;

  
  /**
    A storage to cache the global system variable's value.
    Handling of a separate event will be governed its member.
  */
  ulong rbr_exec_mode;

  /**
    Defines the type of the cache, if any, where the event will be
    stored before being flushed to disk.
  */
  enum_event_cache_type event_cache_type;

  /**
    Defines when information, i.e. event or cache, will be flushed
    to disk.
  */
  enum_event_logging_type event_logging_type;
  /**
    Placeholder for event checksum while writing to binlog.
  */
  ha_checksum crc;
  /**
    Index in @c rli->gaq array to indicate a group that this event is
    purging. The index is set by Coordinator to a group terminator
    event is checked by Worker at the event execution. The indexed
    data represent the Worker progress status.
  */
  ulong mts_group_idx;
  /**
   The Log_event_header class contains the variable present
   in the common header
  */

  binary_log::Log_event_header *common_header;
  /**
    MTS: associating the event with either an assigned Worker or Coordinator.
    Additionally the member serves to tag deferred (IRU) events to avoid
    the event regular time destruction.
  */
  Relay_log_info *worker;

  /** 
    A copy of the main rli value stored into event to pass to MTS worker rli
  */
  ulonglong future_event_relay_log_pos;

#ifdef MYSQL_SERVER
  THD* thd;
  /**
     Partition info associate with event to deliver to MTS event applier 
  */
  db_worker_hash_entry *mts_assigned_partitions[MAX_DBS_IN_EVENT_MTS];

  Log_event(enum_event_cache_type cache_type_arg= EVENT_INVALID_CACHE,
            enum_event_logging_type logging_type_arg= EVENT_INVALID_LOGGING);
  Log_event(THD* thd_arg, uint16 flags_arg,
            enum_event_cache_type cache_type_arg,
            enum_event_logging_type logging_type_arg);
  /*
    read_log_event() functions read an event from a binlog or relay
    log; used by SHOW BINLOG EVENTS, the binlog_dump thread on the
    master (reads master's binlog), the slave IO thread (reads the
    event sent by binlog_dump), the slave SQL thread (reads the event
    from the relay log).  If mutex is 0, the read will proceed without
    mutex.  We need the description_event to be able to parse the
    event (to know the post-header's size); in fact in read_log_event
    we detect the event's type, then call the specific event's
    constructor and pass description_event as an argument.
  */
  static Log_event* read_log_event(IO_CACHE* file,
                                   mysql_mutex_t* log_lock,
                                   const Format_description_log_event
                                   *description_event,
                                   my_bool crc_check);

  /**
    Reads an event from a binlog or relay log. Used by the dump thread
    this method reads the event into a raw buffer without parsing it.

    @Note If mutex is 0, the read will proceed without mutex.

    @Note If a log name is given than the method will check if the
    given binlog is still active.

    @param[in]  file                log file to be read
    @param[out] packet              packet to hold the event
    @param[in]  lock                the lock to be used upon read
    @param[in]  checksum_alg_arg    the checksum algorithm
    @param[in]  log_file_name_arg   the log's file name
    @param[out] is_binlog_active    is the current log still active

    @retval 0                   success
    @retval LOG_READ_EOF        end of file, nothing was read
    @retval LOG_READ_BOGUS      malformed event
    @retval LOG_READ_IO         io error while reading
    @retval LOG_READ_MEM        packet memory allocation failed
    @retval LOG_READ_TRUNC      only a partial event could be read
    @retval LOG_READ_TOO_LARGE  event too large
   */
  static int read_log_event(IO_CACHE* file, String* packet,
                            mysql_mutex_t* log_lock,
                            enum_binlog_checksum_alg checksum_alg_arg,
                            const char *log_file_name_arg= NULL,
                            bool* is_binlog_active= NULL);
  /*
    init_show_field_list() prepares the column names and types for the
    output of SHOW BINLOG EVENTS; it is used only by SHOW BINLOG
    EVENTS.
  */
  static void init_show_field_list(List<Item>* field_list);
#ifdef HAVE_REPLICATION
  int net_send(Protocol *protocol, const char* log_name, my_off_t pos);

  /**
    Stores a string representation of this event in the Protocol.
    This is used by SHOW BINLOG EVENTS.

    @retval 0 success
    @retval nonzero error
  */
  virtual int pack_info(Protocol *protocol);

#endif /* HAVE_REPLICATION */
  virtual const char* get_db()
  {
    return thd ? thd->db : 0;
  }
#else // ifdef MYSQL_SERVER
  Log_event(enum_event_cache_type cache_type_arg= EVENT_INVALID_CACHE,
            enum_event_logging_type logging_type_arg= EVENT_INVALID_LOGGING)
  : temp_buf(0),  event_cache_type(cache_type_arg),
    event_logging_type(logging_type_arg), common_header(0)
  {
    common_header= new Log_event_header();
  }
    /* avoid having to link mysqlbinlog against libpthread */
  static Log_event* read_log_event(IO_CACHE* file,
                                   const Format_description_log_event
                                   *description_event, my_bool crc_check);
  /* print*() functions are used by mysqlbinlog */
  virtual void print(FILE* file, PRINT_EVENT_INFO* print_event_info) = 0;
  void print_timestamp(IO_CACHE* file, time_t* ts);
  void print_header(IO_CACHE* file, PRINT_EVENT_INFO* print_event_info,
                    bool is_more);
  void print_base64(IO_CACHE* file, PRINT_EVENT_INFO* print_event_info,
                    bool is_more);
#endif // ifdef MYSQL_SERVER ... else

  static void *operator new(size_t size)
  {
    return (void*) my_malloc(key_memory_log_event,
                             (uint)size, MYF(MY_WME|MY_FAE));
  }

  static void operator delete(void *ptr, size_t)
  {
    my_free(ptr);
  }

  /* Placement version of the above operators */
  static void *operator new(size_t, void* ptr) { return ptr; }
  static void operator delete(void*, void*) { }
  bool wrapper_my_b_safe_write(IO_CACHE* file, const uchar* buf, ulong data_length);

#ifdef MYSQL_SERVER
  bool write_header(IO_CACHE* file, ulong data_length);
  bool write_footer(IO_CACHE* file);
  my_bool need_checksum();

  virtual bool write(IO_CACHE* file)
  {
    return(write_header(file, get_data_size()) ||
	   write_data_header(file) ||
	   write_data_body(file) ||
	   write_footer(file));
  }
  virtual bool write_data_header(IO_CACHE* file)
  { return 0; }
  virtual bool write_data_body(IO_CACHE* file __attribute__((unused)))
  { return 0; }
  inline time_t get_time()
  {
    /* Not previously initialized */
    if (!common_header->when.tv_sec && !common_header->when.tv_usec)
    {
      THD *tmp_thd= thd ? thd : current_thd;
      if (tmp_thd)
        common_header->when= tmp_thd->start_time;
      else
        my_micro_time_to_timeval(my_micro_time(), &(common_header->when));
    }
    return (time_t) common_header->when.tv_sec;
  }
#endif
  virtual Log_event_type get_type_code() = 0;
  virtual bool is_valid() const = 0;
  void set_artificial_event() { common_header->flags |= LOG_EVENT_ARTIFICIAL_F; }
  void set_relay_log_event() { common_header->flags |= LOG_EVENT_RELAY_LOG_F; }
  bool is_artificial_event() const { return common_header->flags & LOG_EVENT_ARTIFICIAL_F; }
  bool is_relay_log_event() const { return common_header->flags & LOG_EVENT_RELAY_LOG_F; }
  bool is_ignorable_event() const { return common_header->flags & LOG_EVENT_IGNORABLE_F; }
  bool is_no_filter_event() const { return common_header->flags & LOG_EVENT_NO_FILTER_F; }
  inline bool is_using_trans_cache() const
  {
    return (event_cache_type == EVENT_TRANSACTIONAL_CACHE);
  }
  inline bool is_using_stmt_cache() const
  {
    return(event_cache_type == EVENT_STMT_CACHE);
  }
  inline bool is_using_immediate_logging() const
  {
    return(event_logging_type == EVENT_IMMEDIATE_LOGGING);
  }

  /*
     Added a flag which is set for the events already moved into
     binlogapi. For the events being decoded in BAPI, common_header should
     point to the header object which is contained within the class
    Binary_log_event.
     Once all the events are moved, this parameter would be removed.
  */
  Log_event(Log_event_header *header, bool flag_moved= false);
  /*
     This ctor is added in the process of fixing valgrind faliure it will be
     removed after all the events are moved to libbinlogapi
  */
  Log_event(const char *buf, const Format_description_log_event *description_event);
  virtual ~Log_event()
  {
    free_temp_buf();
    if (common_header)
      delete common_header;
    common_header= 0;
  }
  void register_temp_buf(char* buf) { temp_buf = buf; }
  void free_temp_buf()
  {
    if (temp_buf)
    {
      my_free(temp_buf);
      temp_buf = 0;
    }
  }
  /*
    Get event length for simple events. For complicated events the length
    is calculated during write()
  */
  virtual int get_data_size() { return 0;}
  static Log_event* read_log_event(const char* buf, uint event_len,
				   const char **error,
                                   const Format_description_log_event
                                   *description_event, my_bool crc_check);
  /**
    Returns the human readable name of the given event type.
  */
  static const char* get_type_str(Log_event_type type);
  /**
    Returns the human readable name of this event's type.
  */
  const char* get_type_str();

  /* Return start of query time or current time */

#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)

private:

  /*
    possible decisions by get_mts_execution_mode().
    The execution mode can be PARALLEL or not (thereby sequential
    unless impossible at all). When it's sequential it further  breaks into
    ASYNChronous and SYNChronous.
  */
  enum enum_mts_event_exec_mode
  {
    /*
      Event is run by a Worker.
    */
    EVENT_EXEC_PARALLEL,
    /*
      Event is run by Coordinator.
    */
    EVENT_EXEC_ASYNC,
    /*
      Event is run by Coordinator and requires synchronization with Workers.
    */
    EVENT_EXEC_SYNC,
    /*
      Event can't be executed neither by Workers nor Coordinator.
    */
    EVENT_EXEC_CAN_NOT
  };

  /**
     Is called from get_mts_execution_mode() to

     @return TRUE  if the event needs applying with synchronization
                   agaist Workers, otherwise
             FALSE

     @note There are incompatile combinations such as referred further events
           are wrapped with BEGIN/COMMIT. Such cases should be identified
           by the caller and treats correspondingly.

           todo: to mts-support Old master Load-data related events
  */
  bool is_mts_sequential_exec()
  {
    return
      get_type_code() == START_EVENT_V3          ||
      get_type_code() == STOP_EVENT              ||
      get_type_code() == ROTATE_EVENT            ||
      get_type_code() == LOAD_EVENT              ||
      get_type_code() == SLAVE_EVENT             ||
      get_type_code() == CREATE_FILE_EVENT       ||
      get_type_code() == DELETE_FILE_EVENT       ||
      get_type_code() == NEW_LOAD_EVENT          ||
      get_type_code() == EXEC_LOAD_EVENT         ||
      get_type_code() == FORMAT_DESCRIPTION_EVENT||

      get_type_code() == INCIDENT_EVENT;
  }

  /**
     MTS Coordinator finds out a way how to execute the current event.

     Besides the parallelizable case, some events have to be applied by
     Coordinator concurrently with Workers and some to require synchronization
     with Workers (@c see wait_for_workers_to_finish) before to apply them.

     @retval EVENT_EXEC_PARALLEL  if event is executed by a Worker
     @retval EVENT_EXEC_ASYNC     if event is executed by Coordinator
     @retval EVENT_EXEC_SYNC      if event is executed by Coordinator
                                  with synchronization against the Workers
  */
  enum enum_mts_event_exec_mode get_mts_execution_mode(ulong slave_server_id,
                                                   bool mts_in_group)
  {
    if ((get_type_code() == FORMAT_DESCRIPTION_EVENT &&
         ((server_id == (uint32) ::server_id) || (common_header->log_pos == 0))) ||
        (get_type_code() == ROTATE_EVENT &&
         ((server_id == (uint32) ::server_id) ||
          (common_header->log_pos == 0    /* very first fake Rotate (R_f) */
           && mts_in_group /* ignored event turned into R_f at slave stop */))))
      return EVENT_EXEC_ASYNC;
    else if (is_mts_sequential_exec())
      return EVENT_EXEC_SYNC;
    else
      return EVENT_EXEC_PARALLEL;
  }

  /**
     @return index  in \in [0, M] range to indicate
             to be assigned worker;
             M is the max index of the worker pool.
  */
  Slave_worker *get_slave_worker(Relay_log_info *rli);

  /*
    The method returns a list of updated by the event databases.
    Other than in the case of Query-log-event the list is just one item.
  */
  virtual List<char>* get_mts_dbs(MEM_ROOT *mem_root)
  {
    List<char> *res= new List<char>;
    res->push_back(strdup_root(mem_root, get_db()));
    return res;
  }

  /*
    Group of events can be marked to force its execution
    in isolation from any other Workers.
    Typically that is done for a transaction that contains 
    a query accessing more than OVER_MAX_DBS_IN_EVENT_MTS databases.
    Factually that's a sequential mode where a Worker remains to
    be the applier.
  */
  virtual void set_mts_isolate_group()
  { 
    DBUG_ASSERT(ends_group() ||
                get_type_code() == QUERY_EVENT ||
                get_type_code() == EXEC_LOAD_EVENT ||
                get_type_code() == EXECUTE_LOAD_QUERY_EVENT);
    common_header->flags |= LOG_EVENT_MTS_ISOLATE_F;
  }


public:

  /**
     @return TRUE  if events carries partitioning data (database names).
  */
  bool contains_partition_info(bool);

  /*
    @return  the number of updated by the event databases.

    @note In other than Query-log-event case that's one.
  */
  virtual uint8 mts_number_dbs() { return 1; }

  /**
    @return TRUE  if the terminal event of a group is marked to
                  execute in isolation from other Workers,
            FASE  otherwise
  */
  bool is_mts_group_isolated() { return common_header->flags &
                                        LOG_EVENT_MTS_ISOLATE_F; }

  /**
     Events of a certain type can start or end a group of events treated
     transactionally wrt binlog.

     Public access is required by implementation of recovery + skip.

     @return TRUE  if the event starts a group (transaction)
             FASE  otherwise
  */
  virtual bool starts_group() { return FALSE; }

  /**
     @return TRUE  if the event ends a group (transaction)
             FASE  otherwise
  */
  virtual bool ends_group()   { return FALSE; }

  /**
     Apply the event to the database.

     This function represents the public interface for applying an
     event.

     @see do_apply_event
   */
  int apply_event(Relay_log_info *rli);

  /**
     Update the relay log position.

     This function represents the public interface for "stepping over"
     the event and will update the relay log information.

     @see do_update_pos
   */
  int update_pos(Relay_log_info *rli)
  {
    return do_update_pos(rli);
  }

  /**
     Decide if the event shall be skipped, and the reason for skipping
     it.

     @see do_shall_skip
   */
  enum_skip_reason shall_skip(Relay_log_info *rli)
  {
    return do_shall_skip(rli);
  }

  /**
    Primitive to apply an event to the database.

    This is where the change to the database is made.

    @note The primitive is protected instead of private, since there
    is a hierarchy of actions to be performed in some cases.

    @see Format_description_log_event::do_apply_event()

    @param rli Pointer to relay log info structure

    @retval 0     Event applied successfully
    @retval errno Error code if event application failed
  */
  virtual int do_apply_event(Relay_log_info const *rli)
  {
    return 0;                /* Default implementation does nothing */
  }

  virtual int do_apply_event_worker(Slave_worker *w);

protected:

  /**
     Helper function to ignore an event w.r.t. the slave skip counter.

     This function can be used inside do_shall_skip() for functions
     that cannot end a group. If the slave skip counter is 1 when
     seeing such an event, the event shall be ignored, the counter
     left intact, and processing continue with the next event.

     A typical usage is:
     @code
     enum_skip_reason do_shall_skip(Relay_log_info *rli) {
       return continue_group(rli);
     }
     @endcode

     @return Skip reason
   */
  enum_skip_reason continue_group(Relay_log_info *rli);

  /**
     Advance relay log coordinates.

     This function is called to advance the relay log coordinates to
     just after the event.  It is essential that both the relay log
     coordinate and the group log position is updated correctly, since
     this function is used also for skipping events.

     Normally, each implementation of do_update_pos() shall:

     - Update the event position to refer to the position just after
       the event.

     - Update the group log position to refer to the position just
       after the event <em>if the event is last in a group</em>

     @param rli Pointer to relay log info structure

     @retval 0     Coordinates changed successfully
     @retval errno Error code if advancing failed (usually just
                   1). Observe that handler errors are returned by the
                   do_apply_event() function, and not by this one.
   */
  virtual int do_update_pos(Relay_log_info *rli);


  /**
     Decide if this event shall be skipped or not and the reason for
     skipping it.

     The default implementation decide that the event shall be skipped
     if either:

     - the server id of the event is the same as the server id of the
       server and <code>rli->replicate_same_server_id</code> is true,
       or

     - if <code>rli->slave_skip_counter</code> is greater than zero.

     @see do_apply_event
     @see do_update_pos

     @retval Log_event::EVENT_SKIP_NOT
     The event shall not be skipped and should be applied.

     @retval Log_event::EVENT_SKIP_IGNORE
     The event shall be skipped by just ignoring it, i.e., the slave
     skip counter shall not be changed. This happends if, for example,
     the originating server id of the event is the same as the server
     id of the slave.

     @retval Log_event::EVENT_SKIP_COUNT
     The event shall be skipped because the slave skip counter was
     non-zero. The caller shall decrease the counter by one.
   */
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli);
#endif
};


/*
   One class for each type of event.
   Two constructors for each class:
   - one to create the event for logging (when the server acts as a master),
   called after an update to the database is done,
   which accepts parameters like the query, the database, the options for LOAD
   DATA INFILE...
   - one to create the event from a packet (when the server acts as a slave),
   called before reproducing the update, which accepts parameters (like a
   buffer). Used to read from the master, from the relay log, and in
   mysqlbinlog. This constructor must be format-tolerant.
*/

/**
  @class Query_log_event

  A @Query event is written to the binary log whenever the databse is
  modified on the master, unless row based logging is used.

  Query_log_event is created for logging, called after an update to the
  database is done. It is used when the server acts as the master.

  The inheritance structure is as follows:

                Binary_log_event
                     /   \
        <<virtual>> /     \ <<virtual>>
                   /       \
            Query_event  Log_event
                   \       /
                    \     /
                     \   /
                Query_log_event

  TODO: Remove virtual inheritance once all the events are implemented in
        libbinlogapi
*/
class Query_log_event: public Log_event, public virtual Query_event
{
  const char* user;
  const char* host;
protected:
  Log_event_header::Byte* data_buf;
public:
  const char* query;
  const char* catalog;
  const char* db;
  /*
    For events created by Query_log_event::do_apply_event (and
    Load_log_event::do_apply_event()) we need the *original* thread
    id, to be able to log the event with the original (=master's)
    thread id (fix for BUG#1686).
  */
  ulong slave_proxy_id;

  const char *time_zone_str;

#ifdef MYSQL_SERVER

  Query_log_event(THD* thd_arg, const char* query_arg, ulong query_length,
                  bool using_trans, bool immediate, bool suppress_use,
                  int error, bool ignore_command= FALSE);
  const char* get_db() { return db; }

  /**
     Returns a list of updated databases or the default db single item list
     in case of the number of databases exceeds MAX_DBS_IN_EVENT_MTS.
  */
  virtual List<char>* get_mts_dbs(MEM_ROOT *mem_root)
  {
    List<char> *res= new (mem_root) List<char>;
    if (mts_accessed_dbs == OVER_MAX_DBS_IN_EVENT_MTS)
    {
      // the empty string db name is special to indicate sequential applying
      mts_accessed_db_names[0][0]= 0;
      res->push_back((char*) mts_accessed_db_names[0]);
    }
    else
    {
      for (uchar i= 0; i < mts_accessed_dbs; i++)
      {
        char *db_name= mts_accessed_db_names[i];

        // Only default database is rewritten.
        if (!rpl_filter->is_rewrite_empty() && !strcmp(get_db(), db_name))
        {
          size_t dummy_len;
          const char *db_filtered= rpl_filter->get_rewrite_db(db_name, &dummy_len);
          // db_name != db_filtered means that db_name is rewritten.
          if (strcmp(db_name, db_filtered))
            db_name= (char*)db_filtered;
        }

        res->push_back(db_name);
      }
    }
    return res;
  }

  void attach_temp_tables_worker(THD*, const Relay_log_info *);
  void detach_temp_tables_worker(THD*, const Relay_log_info *);

  virtual uchar mts_number_dbs() { return mts_accessed_dbs; }

#ifdef HAVE_REPLICATION
  int pack_info(Protocol* protocol);
#endif /* HAVE_REPLICATION */
#else
  void print_query_header(IO_CACHE* file, PRINT_EVENT_INFO* print_event_info);
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info);
#endif

  Query_log_event();
  Query_log_event(const char* buf, uint event_len,
                  const Format_description_event *description_event,
                  Log_event_type event_type);
  ~Query_log_event()
  {
    if (data_buf)
      my_free(data_buf);
  }
  Log_event_type get_type_code() { return Query_event::get_type_code(); }
#ifdef MYSQL_SERVER
  bool write(IO_CACHE* file);
  virtual bool write_post_header_for_derived(IO_CACHE* file) { return FALSE; }
#endif
  bool is_valid() const { return Query_event::is_valid(); }

  /*
    Returns number of bytes additionaly written to post header by derived
    events (so far it is only Execute_load_query event).
  */
  virtual ulong get_post_header_size_for_derived() { return 0; }
  /* Writes derived event-specific part of post header. */

public:        /* !!! Public in this patch to allow old usage */
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli);
  virtual int do_apply_event(Relay_log_info const *rli);
  virtual int do_update_pos(Relay_log_info *rli);

  int do_apply_event(Relay_log_info const *rli,
                       const char *query_arg,
                       uint32 q_len_arg);
#endif /* HAVE_REPLICATION */
  /*
    If true, the event always be applied by slave SQL thread or be printed by
    mysqlbinlog
   */
  bool is_trans_keyword()
  {
    /*
      Before the patch for bug#50407, The 'SAVEPOINT and ROLLBACK TO'
      queries input by user was written into log events directly.
      So the keywords can be written in both upper case and lower case
      together, strncasecmp is used to check both cases. they also could be
      binlogged with comments in the front of these keywords. for examples:
        / * bla bla * / SAVEPOINT a;
        / * bla bla * / ROLLBACK TO a;
      but we don't handle these cases and after the patch, both quiries are
      binlogged in upper case with no comments.
     */
    return !strncmp(query, "BEGIN", q_len) ||
      !strncmp(query, "COMMIT", q_len) ||
      !strncasecmp(query, "SAVEPOINT", 9) ||
      !strncasecmp(query, "ROLLBACK", 8);
  }
  /*
    Prepare and commit sequence number. will be set to 0 if the event is not a
    transaction starter.
  int64 commit_seq_no;
   */
  /**
     Notice, DDL queries are logged without BEGIN/COMMIT parentheses
     and identification of such single-query group
     occures within logics of @c get_slave_worker().
  */
  bool starts_group() { return !strncmp(query, "BEGIN", q_len); }
  virtual bool ends_group()
  {  
    return
      !strncmp(query, "COMMIT", q_len) ||
      (!strncasecmp(query, STRING_WITH_LEN("ROLLBACK"))
       && strncasecmp(query, STRING_WITH_LEN("ROLLBACK TO ")));
  }
};


/**
  @class Load_log_event

  This log event corresponds to a "LOAD DATA INFILE" SQL query.
  it is a subclass of Rotate_event, defined in binlogapi, and is used
  by the slave to execute the LOAD DATA INFILE query, as a series of events.

  This event type is understood by current versions, but only
  generated by MySQL 3.23 and earlier.

  The inheritance structure in the current design for the classes is
  as follows:

                Binary_log_event
                     /   \
        <<virtual>> /     \ <<virtual>>
                   /       \
             Load_event  Log_event
                   \       /
                    \     /
                     \   /
                 Load_log_event

  TODO: Remove virtual inheritance once all the events are implemented in
        libbinlogapi
*/
class Load_log_event: public Log_event, public virtual Load_event
{
private:
protected:
  int copy_log_event(const char *buf, ulong event_len,
                     int body_offset,
                     const Format_description_event* description_event);
public:
  uint get_query_buffer_length();
  void print_query(bool need_db, const char *cs, char *buf, char **end,
                   char **fn_start, char **fn_end);
  ulong thread_id;
  sql_ex_info sql_ex;

  /* fname doesn't point to memory inside Log_event::temp_buf  */
  void set_fname_outside_temp_buf(const char *afname, uint alen)
  {
    fname= afname;
    fname_len= alen;
    local_fname= TRUE;
  }
  /* fname doesn't point to memory inside Log_event::temp_buf  */
  int  check_fname_outside_temp_buf()
  {
    return local_fname;
  }

#ifdef MYSQL_SERVER
  String field_lens_buf;
  String fields_buf;

  Load_log_event(THD* thd, sql_exchange* ex, const char* db_arg,
		 const char* table_name_arg,
		 List<Item>& fields_arg,
                 bool is_concurrent_arg,
                 enum enum_duplicates handle_dup, bool ignore,
		 bool using_trans);
  void set_fields(const char* db, List<Item> &fields_arg,
                  Name_resolution_context *context);
  const char* get_db() { return db; }
#ifdef HAVE_REPLICATION
  int pack_info(Protocol* protocol);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info);
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info, bool commented);
#endif

  /*
    Note that for all the events related to LOAD DATA (Load_log_event,
    Create_file/Append/Exec/Delete, we pass description_event; however as
    logging of LOAD DATA is going to be changed in 4.1 or 5.0, this is only used
    for the common_header_len (post_header_len will not be changed).
  */
  Load_log_event(const char* buf, uint event_len,
                 const Format_description_event* description_event);
  ~Load_log_event()
  {}
  Log_event_type get_type_code()
  {
    return sql_ex.data_info.new_format() ? NEW_LOAD_EVENT: LOAD_EVENT;
  }
#ifdef MYSQL_SERVER
  bool write_data_header(IO_CACHE* file);
  bool write_data_body(IO_CACHE* file);
#endif
  virtual bool is_valid() const { return Load_event::is_valid(); }
  int get_data_size()
  {
    return (table_name_len + db_len + 2 + fname_len
	    + Binary_log_event::LOAD_HEADER_LEN
            + sql_ex.data_info.data_size() + field_block_len + num_fields);
  }

public:        /* !!! Public in this patch to allow old usage */
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const* rli)
  {
    return do_apply_event(thd->slave_net,rli,0);
  }

  int do_apply_event(NET *net, Relay_log_info const *rli,
                     bool use_rli_only_for_errors);
#endif
};


/**
  @class Start_log_event_v3

  Start_log_event_v3 is the Start_log_event of binlog format 3 (MySQL 3.23 and
  4.x).

  Format_description_log_event derives from Start_log_event_v3; it is
  the Start_log_event of binlog format 4 (MySQL 5.0), that is, the
  event that describes the other events' Common-Header/Post-Header
  lengths. This event is sent by MySQL 5.0 whenever it starts sending
  a new binlog if the requested position is >4 (otherwise if ==4 the
  event will be sent naturally).

  @section Start_log_event_v3_binary_format Binary Format
*/
class Start_log_event_v3: public Log_event
{
public:
  /*
    If this event is at the start of the first binary log since server
    startup 'created' should be the timestamp when the event (and the
    binary log) was created.  In the other case (i.e. this event is at
    the start of a binary log created by FLUSH LOGS or automatic
    rotation), 'created' should be 0.  This "trick" is used by MySQL
    >=4.0.14 slaves to know whether they must drop stale temporary
    tables and whether they should abort unfinished transaction.

    Note that when 'created'!=0, it is always equal to the event's
    timestamp; indeed Start_log_event is written only in log.cc where
    the first constructor below is called, in which 'created' is set
    to 'when'.  So in fact 'created' is a useless variable. When it is
    0 we can read the actual value from timestamp ('when') and when it
    is non-zero we can read the same value from timestamp
    ('when'). Conclusion:
     - we use timestamp to print when the binlog was created.
     - we use 'created' only to know if this is a first binlog or not.
     In 3.23.57 we did not pay attention to this identity, so mysqlbinlog in
     3.23.57 does not print 'created the_date' if created was zero. This is now
     fixed.
  */
  time_t created;
  uint16 binlog_version;
  char server_version[ST_SERVER_VER_LEN];
  /*
    We set this to 1 if we don't want to have the created time in the log,
    which is the case when we rollover to a new log.
  */
  bool dont_set_created;

#ifdef MYSQL_SERVER
  Start_log_event_v3();
#ifdef HAVE_REPLICATION
  int pack_info(Protocol* protocol);
#endif /* HAVE_REPLICATION */
#else
  Start_log_event_v3() {}
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info);
#endif

  Start_log_event_v3(const char* buf,
                     const Format_description_log_event* description_event);
  ~Start_log_event_v3() {}
  Log_event_type get_type_code() { return START_EVENT_V3;}
#ifdef MYSQL_SERVER
  bool write(IO_CACHE* file);
#endif
  bool is_valid() const { return 1; }
  int get_data_size()
  {
    return Binary_log_event::START_V3_HEADER_LEN; //no variable-sized part
  }

protected:
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const *rli);
  virtual enum_skip_reason do_shall_skip(Relay_log_info*)
  {
    /*
      Events from ourself should be skipped, but they should not
      decrease the slave skip counter.
     */
    if (this->server_id == ::server_id)
      return Log_event::EVENT_SKIP_IGNORE;
    else
      return Log_event::EVENT_SKIP_NOT;
  }
#endif
};


/**
  @class Format_description_log_event

  For binlog version 4.
  This event is saved by threads which read it, as they need it for future
  use (to decode the ordinary events).

  @section Format_description_log_event_binary_format Binary Format
*/

class Format_description_log_event: public Start_log_event_v3
{
public:
  /*
     The size of the fixed header which _all_ events have
     (for binlogs written by this version, this is equal to
     LOG_EVENT_HEADER_LEN), except FORMAT_DESCRIPTION_EVENT and ROTATE_EVENT
     (those have a header of size LOG_EVENT_MINIMAL_HEADER_LEN).
  */
  uint8 common_header_len;
  uint8 number_of_event_types;
  /* 
     The list of post-headers' lengths followed 
     by the checksum alg decription byte
  */
  uint8 *post_header_len;
  uchar server_version_split[3];
  const uint8 *event_type_permutation;

  Format_description_log_event(uint16 binlog_ver, const char* server_ver=0);
  Format_description_log_event(const char* buf, uint event_len,
                               const Format_description_log_event
                               *description_event);
  ~Format_description_log_event()
  {
    my_free(post_header_len);
  }
  Log_event_type get_type_code() { return FORMAT_DESCRIPTION_EVENT;}
#ifdef MYSQL_SERVER
  bool write(IO_CACHE* file);
#endif
  bool header_is_valid() const
  {
    return ((common_header_len >= ((binlog_version==1) ? OLD_HEADER_LEN :
                                   LOG_EVENT_MINIMAL_HEADER_LEN)) &&
            (post_header_len != NULL));
  }

  bool version_is_valid() const
  {
    /* It is invalid only when all version numbers are 0 */
    return !(server_version_split[0] == 0 &&
             server_version_split[1] == 0 &&
             server_version_split[2] == 0);
  }

  bool is_valid() const
  {
    return header_is_valid() && version_is_valid();
  }

  int get_data_size()
  {
    /*
      The vector of post-header lengths is considered as part of the
      post-header, because in a given version it never changes (contrary to the
      query in a Query_log_event).
    */
    return Binary_log_event::FORMAT_DESCRIPTION_HEADER_LEN;
  }

  void calc_server_version_split();
  ulong get_version_product() const;
  bool is_version_before_checksum() const;
protected:
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const *rli);
  virtual int do_update_pos(Relay_log_info *rli);
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli);
#endif
};


/**
  @class Intvar_log_event

  The class derives from the class Int_var_event in Binlog API,
  defined in the header binlog_event.h. An Intvar_log_event is
  created just before a Query_log_event, if the query uses one
  of the variables LAST_INSERT_ID or INSERT_ID. This class is used
  by the lave for applying the event.
*/
class Intvar_log_event: public Log_event, public Int_var_event
{
public:

#ifdef MYSQL_SERVER
  Intvar_log_event(THD* thd_arg, uchar type_arg, ulonglong val_arg,
                   enum_event_cache_type cache_type_arg,
                   enum_event_logging_type logging_type_arg)
    :Log_event(thd_arg, 0, cache_type_arg, logging_type_arg),
     Int_var_event(type_arg, val_arg) { }
#ifdef HAVE_REPLICATION
  int pack_info(Protocol* protocol);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info);
#endif

  Intvar_log_event(const char* buf,
                   const Format_description_event *description_event);
  ~Intvar_log_event() {}
  Log_event_type get_type_code() { return INTVAR_EVENT;}
  int get_data_size() { return  9; /* sizeof(type) + sizeof(val) */;}
#ifdef MYSQL_SERVER
  bool write(IO_CACHE* file);
#endif
  bool is_valid() const { return 1; }

private:
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const *rli);
  virtual int do_update_pos(Relay_log_info *rli);
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli);
#endif
};


/**
  @class Rand_log_event

  Logs random seed used by the next RAND(), and by PASSWORD() in 4.1.0.
  4.1.1 does not need it (it's repeatable again) so this event needn't be
  written in 4.1.1 for PASSWORD() (but the fact that it is written is just a
  waste, it does not cause bugs).

  The state of the random number generation consists of 128 bits,
  which are stored internally as two 64-bit numbers.
  The inheritance structure in the current design for the classes is
  as follows:
              Binary_log_event
                     /   \
        <<virtual>> /     \ <<virtual>>
                   /       \
           Rand_event  Log_event
                   \       /
                    \     /
                     \   /
                 Rand_log_event

  TODO: Remove virtual inheritance once all the events are implemented in
        libbinlogapi
  @section Rand_log_event_binary_format Binary Format  

*/

class Rand_log_event: public Log_event, public Rand_event
{
 public:

#ifdef MYSQL_SERVER
  Rand_log_event(THD* thd_arg, ulonglong seed1_arg, ulonglong seed2_arg,
                 enum_event_cache_type cache_type_arg,
                 enum_event_logging_type logging_type_arg)
    :Log_event(thd_arg, 0, cache_type_arg, logging_type_arg),
     Rand_event(seed1_arg, seed2_arg) { }
#ifdef HAVE_REPLICATION
  int pack_info(Protocol* protocol);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info);
#endif

  Rand_log_event(const char* buf,
                 const Format_description_event *description_event);
  ~Rand_log_event() {}
  Log_event_type get_type_code() { return RAND_EVENT;}
  int get_data_size() { return 16; /* sizeof(ulonglong) * 2*/ }
#ifdef MYSQL_SERVER
  bool write(IO_CACHE* file);
#endif
  bool is_valid() const { return 1; }

private:
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const *rli);
  virtual int do_update_pos(Relay_log_info *rli);
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli);
#endif
};

/**
  @class Xid_log_event

  This is the subclass of Xid_event defined in libbinlogapi,
  An XID event is generated for a commit of a transaction that modifies one or
  more tables of an XA-capable storage engine
  The inheritance structure in the current design for the classes is
  as follows:

                Binary_log_event
                     /   \
        <<virtual>> /     \ <<virtual>>
                   /       \
           Xid_event  Log_event
                   \       /
                    \     /
                     \   /
                 Xid_log_event

  TODO: Remove virtual inheritance once all the events are implemented in
        libbinlogapi
*/
#ifdef MYSQL_CLIENT
typedef ulonglong my_xid; // this line is the same as in handler.h
#endif

class Xid_log_event: public Log_event, public Xid_event
{
 public:

#ifdef MYSQL_SERVER
  Xid_log_event(THD* thd_arg, my_xid x)
  : Log_event(thd_arg, 0,
              Log_event::EVENT_TRANSACTIONAL_CACHE,
              Log_event::EVENT_NORMAL_LOGGING)
  {
    xid= x;
  }
#ifdef HAVE_REPLICATION
  int pack_info(Protocol* protocol);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info);
#endif

  Xid_log_event(const char* buf,
                const Format_description_event *description_event);
  ~Xid_log_event() {}
  Log_event_type get_type_code() { return XID_EVENT;}
  int get_data_size() { return sizeof(xid); }
#ifdef MYSQL_SERVER
  bool write(IO_CACHE* file);
#endif
  bool is_valid() const { return 1; }
  virtual bool ends_group() { return TRUE; }
private:
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const *rli);
  virtual int do_apply_event_worker(Slave_worker *rli);
  enum_skip_reason do_shall_skip(Relay_log_info *rli);
  bool do_commit(THD *thd_arg);
#endif
};

/**
  @class User_var_log_event

  Every time a query uses the value of a user variable, a User_var_log_event is
  written before the Query_log_event, to set the user variable.
  The inheritance structure in the current design for the classes is
  as follows:
                Binary_log_event
                     /   \
        <<virtual>> /     \ <<virtual>>
                   /       \
           User_var_event  Log_event
                   \       /
                    \     /
                     \   /
                 User_var_log_event

  TODO: Remove virtual inheritance once all the events are implemented in
        libbinlogapi
  @section User_var_log_event_binary_format Binary Format
*/

class User_var_log_event: public Log_event, public User_var_event
{
public:
#ifdef MYSQL_SERVER
  bool deferred;
  query_id_t query_id;
  User_var_log_event(THD* thd_arg, const char *name_arg, uint name_len_arg,
                     char *val_arg, ulong val_len_arg, Item_result type_arg,
		     uint charset_number_arg, uchar flags_arg,
                     enum_event_cache_type cache_type_arg,
                     enum_event_logging_type logging_type_arg)
    :Log_event(thd_arg, 0, cache_type_arg, logging_type_arg),
     User_var_event(name_arg, name_len_arg, val_arg, val_len_arg,
                    (Value_type)type_arg, charset_number_arg, flags_arg),
     deferred(false)
    { }
  int pack_info(Protocol* protocol);
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info);
#endif

  User_var_log_event(const char* buf, uint event_len,
                     const Format_description_event *description_event);
  ~User_var_log_event() {}
  Log_event_type get_type_code() { return USER_VAR_EVENT;}
#ifdef MYSQL_SERVER
  bool write(IO_CACHE* file);
  /* 
     Getter and setter for deferred User-event. 
     Returns true if the event is not applied directly 
     and which case the applier adjusts execution path.
  */
  bool is_deferred() { return deferred; }
  /*
    In case of the deffered applying the variable instance is flagged
    and the parsing time query id is stored to be used at applying time.
  */
  void set_deferred(query_id_t qid) { deferred= true; query_id= qid; }
#endif
  bool is_valid() const { return name != 0; }

private:
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const *rli);
  virtual int do_update_pos(Relay_log_info *rli);
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli);
#endif
};


/**
  @class Stop_log_event

*/
class Stop_log_event: public Log_event, public Stop_event
{
public:
#ifdef MYSQL_SERVER
  Stop_log_event() :Log_event()
  {}
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info);
#endif

  Stop_log_event(const char* buf,
                 const Format_description_event *description_event):
  Binary_log_event(&buf, description_event->binlog_version), Log_event(this->header())
  {}
  ~Stop_log_event() {}
  Log_event_type get_type_code() { return STOP_EVENT;}
  bool is_valid() const { return 1; }

private:
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_update_pos(Relay_log_info *rli);
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli)
  {
    /*
      Events from ourself should be skipped, but they should not
      decrease the slave skip counter.
     */
    if (this->server_id == ::server_id)
      return Log_event::EVENT_SKIP_IGNORE;
    else
      return Log_event::EVENT_SKIP_NOT;
  }
#endif
};

/**
  @class Rotate_log_event

  This will be deprecated when we move to using sequence ids.
  This class is a subclass of Rotate_event, defined in binlogapi, and is used
  by the slave for updating the position in the relay log.

  It is used by the master inorder to write the rotate event in the binary log.

  The inheritance structure in the current design for the classes is
  as follows:

                Binary_log_event
                     /   \
        <<virtual>> /     \ <<virtual>>
                   /       \
           Rotate_event  Log_event
                   \       /
                    \     /
                     \   /
                 Rotate_log_event

  TODO: Remove virtual inheritance once all the events are implemented in
        libbinlogapi
*/

class Rotate_log_event: public Log_event, public Rotate_event
{
public:
#ifdef MYSQL_SERVER
  Rotate_log_event(const char* new_log_ident_arg,
		   uint ident_len_arg,
		   ulonglong pos_arg, uint flags);
#ifdef HAVE_REPLICATION
  int pack_info(Protocol* protocol);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info);
#endif

  Rotate_log_event(const char* buf, uint event_len,
                   const Format_description_event* description_event);
  ~Rotate_log_event()
  {}
  Log_event_type get_type_code() { return ROTATE_EVENT;}
  int get_data_size() { return  ident_len + Binary_log_event::ROTATE_HEADER_LEN;}
  bool is_valid() const { return Rotate_event::is_valid(); }
#ifdef MYSQL_SERVER
  bool write(IO_CACHE* file);
#endif

private:
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_update_pos(Relay_log_info *rli);
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli);
#endif
};


/* the classes below are for the new LOAD DATA INFILE logging */

/**
  @class Create_file_log_event

  The Create_file_event contains the options to LOAD DATA INFILE.
  This was a design flaw since the file cannot be loaded until the
  Exec_load_event is seen. The use of this event was deprecated from
  MySQL server version 5.0.3 and above.
  To work around this, the slave, when executing the Create_file_log_event,
  writes the Create_file_log_event to a temporary file.

  The inheritance structure is as follows

                    Binary_log_event
                          /   \
                         /     \
                   <<v>>/       \<<v>>
                       /         \
              B_l:Load_event  Log_event
                     /  \        /
               <<v>>/    \<<v>> /
                   /      \    /
                  /        \  /
              B_l:C_F_E  Load_log_event
                  \        /
                   \      /
                    \    /
                     \  /
              Create_file_log_event

  B_l: Namespace Binary_log
  C_F_E: class Create_file_event

  TODO: Remove virtual inheritance once all the events are implemented in
        libbinlogapi

  @section Create_file_log_event_binary_format Binary Format
*/
class Create_file_log_event: public Load_log_event, public Create_file_event
{
public:

#ifdef MYSQL_SERVER
  Create_file_log_event(THD* thd, sql_exchange* ex, const char* db_arg,
			const char* table_name_arg,
			List<Item>& fields_arg,
                        bool is_concurrent_arg,
			enum enum_duplicates handle_dup, bool ignore,
			uchar* block_arg, uint block_len_arg,
			bool using_trans);
#ifdef HAVE_REPLICATION
  int pack_info(Protocol* protocol);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info);
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info,
             bool enable_local);
#endif

  Create_file_log_event(const char* buf, uint event_len,
                        const Format_description_event* description_event);
  ~Create_file_log_event()
  {
  }

  Log_event_type get_type_code()
  {
    return fake_base ? Load_log_event::get_type_code() : CREATE_FILE_EVENT;
  }
  bool is_valid() const { return inited_from_old || block != 0; }
#ifdef MYSQL_SERVER
  bool write_data_header(IO_CACHE* file);
  bool write_data_body(IO_CACHE* file);
  /*
    Cut out Create_file extentions and
    write it as Load event - used on the slave
  */
  bool write_base(IO_CACHE* file);
#endif

private:
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const *rli);
#endif
};


/**
  @class Append_block_log_event

  This event is created to contain the file data. One LOAD_DATA_INFILE
  can have 0 or more instances of this event written to the binary log
  depending on the size of the file.

  The inheritance structure is as follows


                    Binary_log_event
                          /   \
                         /     \
                   <<v>>/       \<<v>>
                       /         \
                  B_l:A_B_E  Log_event
                       \         /
                        \       /
                         \     /
                          \   /
                Append_block_log_event

  B_l: Namespace Binary_log
  A_B_E: class Append_block_event

  TODO: Remove virtual inheritance once all the events are implemented in
        libbinlogapi

  @section Append_block_log_event_binary_format Binary Format
*/
class Append_block_log_event: public Log_event,
                              public virtual Append_block_event
{
public:
#ifdef MYSQL_SERVER
  Append_block_log_event(THD* thd, const char* db_arg, uchar* block_arg,
			 uint block_len_arg, bool using_trans);
#ifdef HAVE_REPLICATION
  int pack_info(Protocol* protocol);
  virtual int get_create_or_append() const;
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info);
#endif

  Append_block_log_event(const char* buf, uint event_len,
                         const Format_description_event
                         *description_event);
  ~Append_block_log_event() {}
  Log_event_type get_type_code() { return APPEND_BLOCK_EVENT;}
  int get_data_size() { return  block_len + Binary_log_event::APPEND_BLOCK_HEADER_LEN ;}
  bool is_valid() const { return Append_block_event::is_valid(); }
#ifdef MYSQL_SERVER
  bool write(IO_CACHE* file);
  const char* get_db() { return db; }
#endif

private:
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const *rli);
#endif
};


/**
  @class Delete_file_log_event

  Delete_file_log_event is created when the LOAD_DATA query fails on the
  master for some reason, and the slave should be notified to abort the
  load. The event is required since the master starts writing the loaded
  block into the binary log before the statement ends. In case of error,
  the slave should abort, and delete any temporary file created while
  applying the (NEW_)LOAD_EVENT.

  The inheritance structure is as follows


                    Binary_log_event
                          /   \
                         /     \
                   <<v>>/       \<<v>>
                       /         \
                  B_l:D_F_E  Log_event
                       \         /
                        \       /
                         \     /
                          \   /
                  Delete_file_log_event

  B_l: Namespace Binary_log
  D_F_E: class Delete_file_event

  TODO: Remove virtual inheritance once all the events are implemented in
        libbinlogapi

  @section Delete_file_log_event_binary_format Binary Format
*/
class Delete_file_log_event: public Log_event, public Delete_file_event
{
public:
  uint file_id;
  const char* db; /* see comment in Append_block_log_event */

#ifdef MYSQL_SERVER
  Delete_file_log_event(THD* thd, const char* db_arg, bool using_trans);
#ifdef HAVE_REPLICATION
  int pack_info(Protocol* protocol);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info);
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info,
             bool enable_local);
#endif

  Delete_file_log_event(const char* buf, uint event_len,
                        const Format_description_event* description_event);
  ~Delete_file_log_event() {}
  Log_event_type get_type_code() { return DELETE_FILE_EVENT;}
  int get_data_size() { return Binary_log_event::DELETE_FILE_HEADER_LEN ;}
  bool is_valid() const { return file_id != 0; }
#ifdef MYSQL_SERVER
  bool write(IO_CACHE* file);
  const char* get_db() { return db; }
#endif

private:
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const *rli);
#endif
};


/**
  @class Execute_load_log_event

  Execute_load_log_event is created when the LOAD_DATA query succeeds on
  the master, The slave should be notified to load the temporary file into
  the table. For server versions > 5.0.3, the temporary files that stores
  the parameters to LOAD DATA INFILE is not needed anymore, since they are
  stored in this event. There is still a temp file containing all the data
  to be loaded.

  The inheritance structure is as follows

                    Binary_log_event
                          /   \
                         /     \
                   <<v>>/       \<<v>>
                       /         \
                  B_l:E_L_E  Log_event
                       \         /
                        \       /
                         \     /
                          \   /
                   Execute_load_log_event

  B_l: Namespace Binary_log
  E_L_E: class Execute_load_event

  TODO: Remove virtual inheritance once all the events are implemented in
        libbinlogapi
  @section Delete_file_log_event_binary_format Binary Format
*/

class Execute_load_log_event: public Log_event, public Execute_load_event
{
public:
#ifdef MYSQL_SERVER
  Execute_load_log_event(THD* thd, const char* db_arg, bool using_trans);
#ifdef HAVE_REPLICATION
  int pack_info(Protocol* protocol);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info);
#endif

  Execute_load_log_event(const char* buf, uint event_len,
                         const Format_description_event
                         *description_event);
  ~Execute_load_log_event() {}
  Log_event_type get_type_code() { return EXEC_LOAD_EVENT;}
  int get_data_size() { return  Binary_log_event::EXEC_LOAD_HEADER_LEN ;}
  bool is_valid() const { return file_id != 0; }
#ifdef MYSQL_SERVER
  bool write(IO_CACHE* file);
  const char* get_db() { return db; }
#endif
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual uint8 mts_number_dbs() { return OVER_MAX_DBS_IN_EVENT_MTS; }
  virtual List<char>* get_mts_dbs(MEM_ROOT *mem_root)
  {
    List<char> *res= new List<char>;
    res->push_back(strdup_root(mem_root, ""));
    return res;
  }
#endif

private:
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const *rli);
#endif
};


/**
  @class Begin_load_query_log_event

  Event for the first block of file to be loaded, its only difference from
  Append_block event is that this event creates or truncates existing file
  before writing data.

  The inheritance structure is as follows

                    Binary_log_event
                          /   \
                         /     \
                   <<v>>/       \<<v>>
                       /         \
                  B_l:A_B_E   Log_event
                     /  \        /
               <<v>>/    \<<v>> /
                   /      \    /
                  /        \  /
            B_l:B_L_Q_E Append_block_event
                  \        /
                   \      /
                    \    /
                     \  /
          Begin_load_query_log_event

  B_l: Namespace Binary_log
  A_B_E: class Append_block_event
  B_L_Q_E: Begin_load_query_event

  TODO: Remove virtual inheritance once all the events are implemented in
        libbinlogapi

  @section Begin_load_query_log_event_binary_format Binary Format
*/
class Begin_load_query_log_event: public Append_block_log_event,
                                  public Begin_load_query_event
{
public:
#ifdef MYSQL_SERVER
  Begin_load_query_log_event(THD* thd_arg, const char *db_arg,
                             uchar* block_arg, uint block_len_arg,
                             bool using_trans);
#ifdef HAVE_REPLICATION
  Begin_load_query_log_event(THD* thd);
  int get_create_or_append() const;
#endif /* HAVE_REPLICATION */
#endif
  Begin_load_query_log_event(const char* buf, uint event_len,
                             const Format_description_event
                             *description_event);
  ~Begin_load_query_log_event() {}
  Log_event_type get_type_code() { return BEGIN_LOAD_QUERY_EVENT; }
private:
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli);
#endif
};


/**
  @class Execute_load_query_log_event

  Event responsible for LOAD DATA execution, it similar to Query_log_event
  but before executing the query it substitutes original filename in LOAD DATA
  query with name of temporary file.

  The inheritance structure is as follows:


                    Binary_log_event
                          /   \
                         /     \
               (1) <<v>>/       \<<v>>(2)
                       /         \
              B_l:Query_event  Log_event
                     /  \        /
               <<v>>/    \<<v>> /
                   /      \    /
                  /        \  /
             B_l:E_L_Q_E Query_log_event
                  \        /
                   \      /
                    \    /
                     \  /
          Execute_load_query_log_event

  B_l: Namespace Binary_log
  E_L_Q_E: class Execute_load_query

  TODO: Remove virtual inheritance (1) and link (2)  once all the events
        are implemented in libbinlogapi

  @section Execute_load_query_log_event_binary_format Binary Format
*/
class Execute_load_query_log_event: public Query_log_event,
                                    public Execute_load_query_event
{
public:

#ifdef MYSQL_SERVER
  Execute_load_query_log_event(THD* thd, const char* query_arg,
                               ulong query_length, uint fn_pos_start_arg,
                               uint fn_pos_end_arg,
                               enum_load_dup_handling dup_handling_arg,
                               bool using_trans, bool immediate,
                               bool suppress_use, int errcode);
#ifdef HAVE_REPLICATION
  int pack_info(Protocol* protocol);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info);
  /* Prints the query as LOAD DATA LOCAL and with rewritten filename */
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info,
             const char *local_fname);
#endif
  Execute_load_query_log_event(const char* buf, uint event_len,
                               const Format_description_event
                               *description_event);
  ~Execute_load_query_log_event() {}

  Log_event_type get_type_code() { return EXECUTE_LOAD_QUERY_EVENT; }
  bool is_valid() const { return Execute_load_query_event::is_valid(); }

  ulong get_post_header_size_for_derived();
#ifdef MYSQL_SERVER
  bool write_post_header_for_derived(IO_CACHE* file);
#endif

private:
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const *rli);
#endif
};


#ifdef MYSQL_CLIENT
/**
  @class Unknown_log_event

  @section Unknown_log_event_binary_format Binary Format
*/
class Unknown_log_event: public Log_event
{
public:
  /*
    Even if this is an unknown event, we still pass description_event to
    Log_event's ctor, this way we can extract maximum information from the
    event's header (the unique ID for example).
  */
  Unknown_log_event(const char* buf,
                    const Format_description_log_event *description_event):
    Log_event(buf, description_event)
  {}
  ~Unknown_log_event() {}
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info);
  Log_event_type get_type_code() { return UNKNOWN_EVENT;}
  bool is_valid() const { return 1; }
};
#endif
char *str_to_hex(char *to, const char *from, uint len);

/**
  @class Table_map_log_event

  In row-based mode, every row operation event is preceded by a
  Table_map_log_event which maps a table definition to a number.  The
  table definition consists of database name, table name, and column
  definitions.

  @section Table_map_log_event_binary_format Binary Format

  The Post-Header has the following components:

  <table>
  <caption>Post-Header for Table_map_log_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>table_id</td>
    <td>6 bytes unsigned integer</td>
    <td>The number that identifies the table.</td>
  </tr>

  <tr>
    <td>flags</td>
    <td>2 byte bitfield</td>
    <td>Reserved for future use; currently always 0.</td>
  </tr>

  </table>

  The Body has the following components:

  <table>
  <caption>Body for Table_map_log_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>database_name</td>
    <td>one byte string length, followed by null-terminated string</td>
    <td>The name of the database in which the table resides.  The name
    is represented as a one byte unsigned integer representing the
    number of bytes in the name, followed by length bytes containing
    the database name, followed by a terminating 0 byte.  (Note the
    redundancy in the representation of the length.)  </td>
  </tr>

  <tr>
    <td>table_name</td>
    <td>one byte string length, followed by null-terminated string</td>
    <td>The name of the table, encoded the same way as the database
    name above.</td>
  </tr>

  <tr>
    <td>column_count</td>
    <td>@ref packed_integer "Packed Integer"</td>
    <td>The number of columns in the table, represented as a packed
    variable-length integer.</td>
  </tr>

  <tr>
    <td>column_type</td>
    <td>List of column_count 1 byte enumeration values</td>
    <td>The type of each column in the table, listed from left to
    right.  Each byte is mapped to a column type according to the
    enumeration type enum_field_types defined in mysql_com.h.  The
    mapping of types to numbers is listed in the table @ref
    Table_table_map_log_event_column_types "below" (along with
    description of the associated metadata field).  </td>
  </tr>

  <tr>
    <td>metadata_length</td>
    <td>@ref packed_integer "Packed Integer"</td>
    <td>The length of the following metadata block</td>
  </tr>

  <tr>
    <td>metadata</td>
    <td>list of metadata for each column</td>
    <td>For each column from left to right, a chunk of data who's
    length and semantics depends on the type of the column.  The
    length and semantics for the metadata for each column are listed
    in the table @ref Table_table_map_log_event_column_types
    "below".</td>
  </tr>

  <tr>
    <td>null_bits</td>
    <td>column_count bits, rounded up to nearest byte</td>
    <td>For each column, a bit indicating whether data in the column
    can be NULL or not.  The number of bytes needed for this is
    int((column_count+7)/8).  The flag for the first column from the
    left is in the least-significant bit of the first byte, the second
    is in the second least significant bit of the first byte, the
    ninth is in the least significant bit of the second byte, and so
    on.  </td>
  </tr>

  </table>

  The table below lists all column types, along with the numerical
  identifier for it and the size and interpretation of meta-data used
  to describe the type.

  @anchor Table_table_map_log_event_column_types
  <table>
  <caption>Table_map_log_event column types: numerical identifier and
  metadata</caption>
  <tr>
    <th>Name</th>
    <th>Identifier</th>
    <th>Size of metadata in bytes</th>
    <th>Description of metadata</th>
  </tr>

  <tr>
    <td>MYSQL_TYPE_DECIMAL</td><td>0</td>
    <td>0</td>
    <td>No column metadata.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_TINY</td><td>1</td>
    <td>0</td>
    <td>No column metadata.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_SHORT</td><td>2</td>
    <td>0</td>
    <td>No column metadata.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_LONG</td><td>3</td>
    <td>0</td>
    <td>No column metadata.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_FLOAT</td><td>4</td>
    <td>1 byte</td>
    <td>1 byte unsigned integer, representing the "pack_length", which
    is equal to sizeof(float) on the server from which the event
    originates.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_DOUBLE</td><td>5</td>
    <td>1 byte</td>
    <td>1 byte unsigned integer, representing the "pack_length", which
    is equal to sizeof(double) on the server from which the event
    originates.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_NULL</td><td>6</td>
    <td>0</td>
    <td>No column metadata.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_TIMESTAMP</td><td>7</td>
    <td>0</td>
    <td>No column metadata.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_LONGLONG</td><td>8</td>
    <td>0</td>
    <td>No column metadata.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_INT24</td><td>9</td>
    <td>0</td>
    <td>No column metadata.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_DATE</td><td>10</td>
    <td>0</td>
    <td>No column metadata.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_TIME</td><td>11</td>
    <td>0</td>
    <td>No column metadata.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_DATETIME</td><td>12</td>
    <td>0</td>
    <td>No column metadata.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_YEAR</td><td>13</td>
    <td>0</td>
    <td>No column metadata.</td>
  </tr>

  <tr>
    <td><i>MYSQL_TYPE_NEWDATE</i></td><td><i>14</i></td>
    <td>&ndash;</td>
    <td><i>This enumeration value is only used internally and cannot
    exist in a binlog.</i></td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_VARCHAR</td><td>15</td>
    <td>2 bytes</td>
    <td>2 byte unsigned integer representing the maximum length of
    the string.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_BIT</td><td>16</td>
    <td>2 bytes</td>
    <td>A 1 byte unsigned int representing the length in bits of the
    bitfield (0 to 64), followed by a 1 byte unsigned int
    representing the number of bytes occupied by the bitfield.  The
    number of bytes is either int((length+7)/8) or int(length/8).</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_NEWDECIMAL</td><td>246</td>
    <td>2 bytes</td>
    <td>A 1 byte unsigned int representing the precision, followed
    by a 1 byte unsigned int representing the number of decimals.</td>
  </tr>

  <tr>
    <td><i>MYSQL_TYPE_ENUM</i></td><td><i>247</i></td>
    <td>&ndash;</td>
    <td><i>This enumeration value is only used internally and cannot
    exist in a binlog.</i></td>
  </tr>

  <tr>
    <td><i>MYSQL_TYPE_SET</i></td><td><i>248</i></td>
    <td>&ndash;</td>
    <td><i>This enumeration value is only used internally and cannot
    exist in a binlog.</i></td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_TINY_BLOB</td><td>249</td>
    <td>&ndash;</td>
    <td><i>This enumeration value is only used internally and cannot
    exist in a binlog.</i></td>
  </tr>

  <tr>
    <td><i>MYSQL_TYPE_MEDIUM_BLOB</i></td><td><i>250</i></td>
    <td>&ndash;</td>
    <td><i>This enumeration value is only used internally and cannot
    exist in a binlog.</i></td>
  </tr>

  <tr>
    <td><i>MYSQL_TYPE_LONG_BLOB</i></td><td><i>251</i></td>
    <td>&ndash;</td>
    <td><i>This enumeration value is only used internally and cannot
    exist in a binlog.</i></td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_BLOB</td><td>252</td>
    <td>1 byte</td>
    <td>The pack length, i.e., the number of bytes needed to represent
    the length of the blob: 1, 2, 3, or 4.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_VAR_STRING</td><td>253</td>
    <td>2 bytes</td>
    <td>This is used to store both strings and enumeration values.
    The first byte is a enumeration value storing the <i>real
    type</i>, which may be either MYSQL_TYPE_VAR_STRING or
    MYSQL_TYPE_ENUM.  The second byte is a 1 byte unsigned integer
    representing the field size, i.e., the number of bytes needed to
    store the length of the string.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_STRING</td><td>254</td>
    <td>2 bytes</td>
    <td>The first byte is always MYSQL_TYPE_VAR_STRING (i.e., 253).
    The second byte is the field size, i.e., the number of bytes in
    the representation of size of the string: 3 or 4.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_GEOMETRY</td><td>255</td>
    <td>1 byte</td>
    <td>The pack length, i.e., the number of bytes needed to represent
    the length of the geometry: 1, 2, 3, or 4.</td>
  </tr>

  </table>
*/
class Table_map_log_event : public Log_event
{
public:
  /* Constants */
  enum
  {
    TYPE_CODE = TABLE_MAP_EVENT
  };

  /**
     Enumeration of the errors that can be returned.
   */
  enum enum_error
  {
    ERR_OPEN_FAILURE = -1,               /**< Failure to open table */
    ERR_OK = 0,                                 /**< No error */
    ERR_TABLE_LIMIT_EXCEEDED = 1,      /**< No more room for tables */
    ERR_OUT_OF_MEM = 2,                         /**< Out of memory */
    ERR_BAD_TABLE_DEF = 3,     /**< Table definition does not match */
    ERR_RBR_TO_SBR = 4  /**< daisy-chanining RBR to SBR not allowed */
  };

  enum enum_flag
  {
    /* 
       Nothing here right now, but the flags support is there in
       preparation for changes that are coming.  Need to add a
       constant to make it compile under HP-UX: aCC does not like
       empty enumerations.
    */
    ENUM_FLAG_COUNT
  };

  typedef uint16 flag_set;

  /* Special constants representing sets of flags */
  enum 
  {
    TM_NO_FLAGS = 0U,
    TM_BIT_LEN_EXACT_F = (1U << 0),
    TM_REFERRED_FK_DB_F = (1U << 1)
  };

  flag_set get_flags(flag_set flag) const { return m_flags & flag; }

#ifdef MYSQL_SERVER
  Table_map_log_event(THD *thd_arg, TABLE *tbl, const Table_id& tid,
                      bool is_transactional);
#endif
#ifdef HAVE_REPLICATION
  Table_map_log_event(const char *buf, uint event_len, 
                      const Format_description_log_event *description_event);
#endif

  ~Table_map_log_event();

#ifdef MYSQL_CLIENT
  table_def *create_table_def()
  {
    return new table_def(m_coltype, m_colcnt, m_field_metadata,
                         m_field_metadata_size, m_null_bits, m_flags);
  }
#endif
  const Table_id& get_table_id() const { return m_table_id; }
  const char *get_table_name() const { return m_tblnam; }
  const char *get_db_name() const    { return m_dbnam; }

  virtual Log_event_type get_type_code() { return TABLE_MAP_EVENT; }
  virtual bool is_valid() const
  {
    return (m_memory != NULL && m_meta_memory != NULL); /* we check malloc */
  }

  virtual int get_data_size() { return (uint) m_data_size; } 
#ifdef MYSQL_SERVER
  virtual int save_field_metadata();
  virtual bool write_data_header(IO_CACHE *file);
  virtual bool write_data_body(IO_CACHE *file);
  virtual const char *get_db() { return m_dbnam; }
  virtual uint8 mts_number_dbs()
  { 
    return get_flags(TM_REFERRED_FK_DB_F) ? OVER_MAX_DBS_IN_EVENT_MTS : 1;
  }
  virtual List<char>* get_mts_dbs(MEM_ROOT *mem_root)
  {
    List<char> *res= new List<char>;
    const char *db_name= get_db();

    if (!rpl_filter->is_rewrite_empty() && !get_flags(TM_REFERRED_FK_DB_F))
    {
      size_t dummy_len;
      const char *db_filtered= rpl_filter->get_rewrite_db(db_name, &dummy_len);
      // db_name != db_filtered means that db_name is rewritten.
      if (strcmp(db_name, db_filtered))
        db_name= db_filtered;
    }

    res->push_back(strdup_root(mem_root,
                               get_flags(TM_REFERRED_FK_DB_F) ? "" : db_name));
    return res;
  }

#endif

#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int pack_info(Protocol *protocol);
#endif

#ifdef MYSQL_CLIENT
  virtual void print(FILE *file, PRINT_EVENT_INFO *print_event_info);
#endif


private:
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const *rli);
  virtual int do_update_pos(Relay_log_info *rli);
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli);
#endif

#ifdef MYSQL_SERVER
  TABLE         *m_table;
#endif
  char const    *m_dbnam;
  size_t         m_dblen;
  char const    *m_tblnam;
  size_t         m_tbllen;
  ulong          m_colcnt;
  uchar         *m_coltype;

  uchar         *m_memory;
  Table_id       m_table_id;
  flag_set       m_flags;

  size_t         m_data_size;

  uchar          *m_field_metadata;        // buffer for field metadata
  /*
    The size of field metadata buffer set by calling save_field_metadata()
  */
  ulong          m_field_metadata_size;   
  uchar         *m_null_bits;
  uchar         *m_meta_memory;
};


/**
  @class Rows_log_event

 Common base class for all row-containing log events.

 RESPONSIBILITIES

   Encode the common parts of all events containing rows, which are:
   - Write data header and data body to an IO_CACHE.
   - Provide an interface for adding an individual row to the event.

  @section Rows_log_event_binary_format Binary Format
*/


class Rows_log_event : public Log_event
{
public:
  enum row_lookup_mode {
       ROW_LOOKUP_UNDEFINED= 0,
       ROW_LOOKUP_NOT_NEEDED= 1,
       ROW_LOOKUP_INDEX_SCAN= 2,
       ROW_LOOKUP_TABLE_SCAN= 3,
       ROW_LOOKUP_HASH_SCAN= 4
  };

  /**
     Enumeration of the errors that can be returned.
   */
  enum enum_error
  {
    ERR_OPEN_FAILURE = -1,               /**< Failure to open table */
    ERR_OK = 0,                                 /**< No error */
    ERR_TABLE_LIMIT_EXCEEDED = 1,      /**< No more room for tables */
    ERR_OUT_OF_MEM = 2,                         /**< Out of memory */
    ERR_BAD_TABLE_DEF = 3,     /**< Table definition does not match */
    ERR_RBR_TO_SBR = 4  /**< daisy-chanining RBR to SBR not allowed */
  };

  /*
    These definitions allow you to combine the flags into an
    appropriate flag set using the normal bitwise operators.  The
    implicit conversion from an enum-constant to an integer is
    accepted by the compiler, which is then used to set the real set
    of flags.
  */
  enum enum_flag
  {
    /* Last event of a statement */
    STMT_END_F = (1U << 0),

    /* Value of the OPTION_NO_FOREIGN_KEY_CHECKS flag in thd->options */
    NO_FOREIGN_KEY_CHECKS_F = (1U << 1),

    /* Value of the OPTION_RELAXED_UNIQUE_CHECKS flag in thd->options */
    RELAXED_UNIQUE_CHECKS_F = (1U << 2),

    /** 
      Indicates that rows in this event are complete, that is contain
      values for all columns of the table.
     */
    COMPLETE_ROWS_F = (1U << 3)
  };

  typedef uint16 flag_set;

  /* Special constants representing sets of flags */
  enum 
  {
      RLE_NO_FLAGS = 0U
  };

  virtual ~Rows_log_event();

  void set_flags(flag_set flags_arg) { m_flags |= flags_arg; }
  void clear_flags(flag_set flags_arg) { m_flags &= ~flags_arg; }
  flag_set get_flags(flag_set flags_arg) const { return m_flags & flags_arg; }

  Log_event_type get_type_code() { return m_type; } /* Specific type (_V1 etc) */
  virtual Log_event_type get_general_type_code() = 0; /* General rows op type, no version */

#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int pack_info(Protocol *protocol);
#endif

#ifdef MYSQL_CLIENT
  /* not for direct call, each derived has its own ::print() */
  virtual void print(FILE *file, PRINT_EVENT_INFO *print_event_info)= 0;
  void print_verbose(IO_CACHE *file,
                     PRINT_EVENT_INFO *print_event_info);
  size_t print_verbose_one_row(IO_CACHE *file, table_def *td,
                               PRINT_EVENT_INFO *print_event_info,
                               MY_BITMAP *cols_bitmap,
                               const uchar *ptr, const uchar *prefix);
#endif

#ifdef MYSQL_SERVER
  int add_row_data(uchar *data, size_t length)
  {
    return do_add_row_data(data,length); 
  }
#endif

  /* Member functions to implement superclass interface */
  virtual int get_data_size();

  MY_BITMAP const *get_cols() const { return &m_cols; }
  MY_BITMAP const *get_cols_ai() const { return &m_cols_ai; }
  size_t get_width() const          { return m_width; }
  const Table_id& get_table_id() const        { return m_table_id; }

#if defined(MYSQL_SERVER)
  /*
    This member function compares the table's read/write_set
    with this event's m_cols and m_cols_ai. Comparison takes 
    into account what type of rows event is this: Delete, Write or
    Update, therefore it uses the correct m_cols[_ai] according
    to the event type code.

    Note that this member function should only be called for the
    following events:
    - Delete_rows_log_event
    - Write_rows_log_event
    - Update_rows_log_event

    @param[IN] table The table to compare this events bitmaps 
                     against.

    @return TRUE if sets match, FALSE otherwise. (following 
                 bitmap_cmp return logic).

   */
  virtual bool read_write_bitmaps_cmp(TABLE *table)
  {
    bool res= FALSE;

    switch (get_general_type_code())
    {
      case DELETE_ROWS_EVENT:
        res= bitmap_cmp(get_cols(), table->read_set);
        break;
      case UPDATE_ROWS_EVENT:
        res= (bitmap_cmp(get_cols(), table->read_set) &&
              bitmap_cmp(get_cols_ai(), table->write_set));
        break;
      case WRITE_ROWS_EVENT:
        res= bitmap_cmp(get_cols(), table->write_set);
        break;
      default:
        /* 
          We should just compare bitmaps for Delete, Write
          or Update rows events.
        */
        DBUG_ASSERT(0);
    }
    return res;
  }
#endif

#ifdef MYSQL_SERVER
  virtual bool write_data_header(IO_CACHE *file);
  virtual bool write_data_body(IO_CACHE *file);
  virtual const char *get_db() { return m_table->s->db.str; }
#endif
  /*
    Check that malloc() succeeded in allocating memory for the rows
    buffer and the COLS vector. Checking that an Update_rows_log_event
    is valid is done in the Update_rows_log_event::is_valid()
    function.
  */
  virtual bool is_valid() const
  {
    return m_rows_buf && m_cols.bitmap;
  }

  uint     m_row_count;         /* The number of rows added to the event */

  const uchar* get_extra_row_data() const   { return m_extra_row_data; }

protected:
  /* 
     The constructors are protected since you're supposed to inherit
     this class, not create instances of this class.
  */
#ifdef MYSQL_SERVER
  Rows_log_event(THD*, TABLE*, const Table_id& table_id,
		 MY_BITMAP const *cols, bool is_transactional,
                 Log_event_type event_type,
                 const uchar* extra_row_info);
#endif
  Rows_log_event(const char *row_data, uint event_len, 
		 const Format_description_log_event *description_event);

#ifdef MYSQL_CLIENT
  void print_helper(FILE *, PRINT_EVENT_INFO *, char const *const name);
#endif

#ifdef MYSQL_SERVER
  virtual int do_add_row_data(uchar *data, size_t length);
#endif

#ifdef MYSQL_SERVER
  TABLE *m_table;		/* The table the rows belong to */
#endif
  Table_id    m_table_id;	/* Table ID */
  MY_BITMAP   m_cols;		/* Bitmap denoting columns available */
  ulong       m_width;          /* The width of the columns bitmap */
#ifndef MYSQL_CLIENT
  /**
     Hash table that will hold the entries for while using HASH_SCAN
     algorithm to search and update/delete rows.
   */
  Hash_slave_rows m_hash;

  /**
     The algorithm to use while searching for rows using the before
     image.
  */
  uint            m_rows_lookup_algorithm;  
#endif
  /*
    Bitmap for columns available in the after image, if present. These
    fields are only available for Update_rows events. Observe that the
    width of both the before image COLS vector and the after image
    COLS vector is the same: the number of columns of the table on the
    master.
  */
  MY_BITMAP   m_cols_ai;

  ulong       m_master_reclength; /* Length of record on master side */

  /* Bit buffers in the same memory as the class */
  uint32    m_bitbuf[128/(sizeof(uint32)*8)];
  uint32    m_bitbuf_ai[128/(sizeof(uint32)*8)];

  uchar    *m_rows_buf;		/* The rows in packed format */
  uchar    *m_rows_cur;		/* One-after the end of the data */
  uchar    *m_rows_end;		/* One-after the end of the allocated space */

  flag_set m_flags;		/* Flags for row-level events */

  Log_event_type m_type;        /* Actual event type */

  uchar    *m_extra_row_data;   /* Pointer to extra row data if any */
                                /* If non null, first byte is length */

  /* helper functions */

#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  const uchar *m_curr_row;     /* Start of the row being processed */
  const uchar *m_curr_row_end; /* One-after the end of the current row */
  uchar    *m_key;      /* Buffer to keep key value during searches */
  uchar    *last_hashed_key;
  uint     m_key_index;
  List<uchar> m_distinct_key_list;
  List_iterator_fast<uchar> m_itr;

  // Unpack the current row into m_table->record[0]
  int unpack_current_row(const Relay_log_info *const rli,
                         MY_BITMAP const *cols)
  { 
    DBUG_ASSERT(m_table);

    ASSERT_OR_RETURN_ERROR(m_curr_row <= m_rows_end, HA_ERR_CORRUPT_EVENT);
    int const result= ::unpack_row(rli, m_table, m_width, m_curr_row, cols,
                                   &m_curr_row_end, &m_master_reclength);
    if (m_curr_row_end > m_rows_end)
      my_error(ER_SLAVE_CORRUPT_EVENT, MYF(0));
    ASSERT_OR_RETURN_ERROR(m_curr_row_end <= m_rows_end, HA_ERR_CORRUPT_EVENT);
    return result;
  }

  /*
    This member function is called when deciding the algorithm to be used to
    find the rows to be updated on the slave during row based replication.
    This this functions sets the m_rows_lookup_algorithm and also the
    m_key_index with the key index to be used if the algorithm is dependent on
    an index.
   */
  void decide_row_lookup_algorithm_and_key();

  /*
    Encapsulates the  operations to be done before applying
    row event for update and delete.
   */
  int row_operations_scan_and_key_setup();

  /*
   Encapsulates the  operations to be done after applying
   row event for update and delete.
  */
  int row_operations_scan_and_key_teardown(int error);

#endif

private:

#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const *rli);
  virtual int do_update_pos(Relay_log_info *rli);
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli);

  /*
    Primitive to prepare for a sequence of row executions.

    DESCRIPTION

      Before doing a sequence of do_prepare_row() and do_exec_row()
      calls, this member function should be called to prepare for the
      entire sequence. Typically, this member function will allocate
      space for any buffers that are needed for the two member
      functions mentioned above.

    RETURN VALUE

      The member function will return 0 if all went OK, or a non-zero
      error code otherwise.
  */
  virtual 
  int do_before_row_operations(const Slave_reporting_capability *const log) = 0;

  /*
    Primitive to clean up after a sequence of row executions.

    DESCRIPTION
    
      After doing a sequence of do_prepare_row() and do_exec_row(),
      this member function should be called to clean up and release
      any allocated buffers.
      
      The error argument, if non-zero, indicates an error which happened during
      row processing before this function was called. In this case, even if 
      function is successful, it should return the error code given in the argument.
  */
  virtual 
  int do_after_row_operations(const Slave_reporting_capability *const log,
                              int error) = 0;

  /*
    Primitive to do the actual execution necessary for a row.

    DESCRIPTION
      The member function will do the actual execution needed to handle a row.
      The row is located at m_curr_row. When the function returns, 
      m_curr_row_end should point at the next row (one byte after the end
      of the current row).    

    RETURN VALUE
      0 if execution succeeded, 1 if execution failed.
      
  */
  virtual int do_exec_row(const Relay_log_info *const rli) = 0;

  /**
    Private member function called while handling idempotent errors.

    @param err[IN/OUT] the error to handle. If it is listed as
                       idempotent/ignored related error, then it is cleared.
    @returns true if the slave should stop executing rows.
   */
  int handle_idempotent_and_ignored_errors(Relay_log_info const *rli, int *err);

  /**
     Private member function called after updating/deleting a row. It
     performs some assertions and more importantly, it updates
     m_curr_row so that the next row is processed during the row
     execution main loop (@c Rows_log_event::do_apply_event()).

     @param err[IN] the current error code.
   */
  void do_post_row_operations(Relay_log_info const *rli, int err);

  /**
     Commodity wrapper around do_exec_row(), that deals with resetting
     the thd reference in the table.
   */
  int do_apply_row(Relay_log_info const *rli);

  /**
     Implementation of the index scan and update algorithm. It uses
     PK, UK or regular Key to search for the record to update. When
     found it updates it.
   */
  int do_index_scan_and_update(Relay_log_info const *rli);
  
  /**
     Implementation of the hash_scan and update algorithm. It collects
     rows positions in a hashtable until the last row is
     unpacked. Then it scans the table to update and when a record in
     the table matches the one in the hashtable, the update/delete is
     performed.
   */
  int do_hash_scan_and_update(Relay_log_info const *rli);

  /**
     Implementation of the legacy table_scan and update algorithm. For
     each unpacked row it scans the storage engine table for a
     match. When a match is found, the update/delete operations are
     performed.
   */
  int do_table_scan_and_update(Relay_log_info const *rli);

  /**
    Initializes scanning of rows. Opens an index and initailizes an iterator
    over a list of distinct keys (m_distinct_key_list) if it is a HASH_SCAN
    over an index or the table if its a HASH_SCAN over the table.
  */
  int open_record_scan();

  /**
    Does the cleanup
    - deallocates all the elements in m_distinct_key_list if any
    - closes the index if opened by open_record_scan
    - closes the table if opened for scanning.
  */
  int close_record_scan();

  /**
    Fetches next row. If it is a HASH_SCAN over an index, it populates
    table->record[0] with the next row corresponding to the index. If
    the indexes are in non-contigous ranges it fetches record corresponding
    to the key value in the next range.

    @parms: bool first_read : signifying if this is the first time we are reading a row
            over an index.
    @return_value: -  error code when there are no more reeords to be fetched or some other
                      error occured,
                   -  0 otherwise.
  */
  int next_record_scan(bool first_read);

  /**
    Populates the m_distinct_key_list with unique keys to be modified
    during HASH_SCAN over keys.
    @return_value -0 success
                  -Err_code
  */
  int add_key_to_distinct_keyset();

  /**
    Populates the m_hash when using HASH_SCAN. Thence, it:
    - unpacks the before image (BI)
    - saves the positions
    - saves the positions into the hash map, using the
      BI checksum as key
    - unpacks the after image (AI) if needed, so that
      m_curr_row_end gets updated correctly.

    @param rli The reference to the relay log info object.
    @returns 0 on success. Otherwise, the error code.
  */
  int do_hash_row(Relay_log_info const *rli);

  /**
    This member function scans the table and applies the changes
    that had been previously hashed. As such, m_hash MUST be filled
    by do_hash_row before calling this member function.

    @param rli The reference to the relay log info object.
    @returns 0 on success. Otherwise, the error code.
  */
  int do_scan_and_update(Relay_log_info const *rli);
#endif /* defined(MYSQL_SERVER) && defined(HAVE_REPLICATION) */

  friend class Old_rows_log_event;
};

/**
  @class Write_rows_log_event

  Log row insertions and updates. The event contain several
  insert/update rows for a table. Note that each event contains only
  rows for one table.

  @section Write_rows_log_event_binary_format Binary Format
*/
class Write_rows_log_event : public Rows_log_event
{
public:
  enum 
  {
    /* Support interface to THD::binlog_prepare_pending_rows_event */
    TYPE_CODE = WRITE_ROWS_EVENT
  };

#if defined(MYSQL_SERVER)
  Write_rows_log_event(THD*, TABLE*, const Table_id& table_id,
		       bool is_transactional,
                       const uchar* extra_row_info);
#endif
#ifdef HAVE_REPLICATION
  Write_rows_log_event(const char *buf, uint event_len, 
                       const Format_description_log_event *description_event);
#endif
#if defined(MYSQL_SERVER) 
  static bool binlog_row_logging_function(THD *thd, TABLE *table,
                                          bool is_transactional,
                                          const uchar *before_record
                                          __attribute__((unused)),
                                          const uchar *after_record)
  {
    return thd->binlog_write_row(table, is_transactional,
                                 after_record, NULL);
  }
#endif

protected:
  int write_row(const Relay_log_info *const, const bool);

private:
  virtual Log_event_type get_general_type_code() { return (Log_event_type)TYPE_CODE; }

#ifdef MYSQL_CLIENT
  void print(FILE *file, PRINT_EVENT_INFO *print_event_info);
#endif

#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_before_row_operations(const Slave_reporting_capability *const);
  virtual int do_after_row_operations(const Slave_reporting_capability *const,int);
  virtual int do_exec_row(const Relay_log_info *const);
#endif
};


/**
  @class Update_rows_log_event

  Log row updates with a before image. The event contain several
  update rows for a table. Note that each event contains only rows for
  one table.

  Also note that the row data consists of pairs of row data: one row
  for the old data and one row for the new data.

  @section Update_rows_log_event_binary_format Binary Format
*/
class Update_rows_log_event : public Rows_log_event
{
public:
  enum 
  {
    /* Support interface to THD::binlog_prepare_pending_rows_event */
    TYPE_CODE = UPDATE_ROWS_EVENT
  };

#ifdef MYSQL_SERVER
  Update_rows_log_event(THD*, TABLE*, const Table_id& table_id,
			MY_BITMAP const *cols_bi,
			MY_BITMAP const *cols_ai,
                        bool is_transactional,
                        const uchar* extra_row_info);

  Update_rows_log_event(THD*, TABLE*, const Table_id& table_id,
                        bool is_transactional,
                        const uchar* extra_row_info);

  void init(MY_BITMAP const *cols);
#endif

  virtual ~Update_rows_log_event();

#ifdef HAVE_REPLICATION
  Update_rows_log_event(const char *buf, uint event_len, 
			const Format_description_log_event *description_event);
#endif

#ifdef MYSQL_SERVER
  static bool binlog_row_logging_function(THD *thd, TABLE *table,
                                          bool is_transactional,
                                          const uchar *before_record,
                                          const uchar *after_record)
  {
    return thd->binlog_update_row(table, is_transactional,
                                  before_record, after_record, NULL);
  }
#endif

  virtual bool is_valid() const
  {
    return Rows_log_event::is_valid() && m_cols_ai.bitmap;
  }

protected:
  virtual Log_event_type get_general_type_code() { return (Log_event_type)TYPE_CODE; }

#ifdef MYSQL_CLIENT
  void print(FILE *file, PRINT_EVENT_INFO *print_event_info);
#endif

#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_before_row_operations(const Slave_reporting_capability *const);
  virtual int do_after_row_operations(const Slave_reporting_capability *const,int);
  virtual int do_exec_row(const Relay_log_info *const);
#endif /* defined(MYSQL_SERVER) && defined(HAVE_REPLICATION) */
};

/**
  @class Delete_rows_log_event

  Log row deletions. The event contain several delete rows for a
  table. Note that each event contains only rows for one table.

  RESPONSIBILITIES

    - Act as a container for rows that has been deleted on the master
      and should be deleted on the slave.

  COLLABORATION

    Row_writer
      Create the event and add rows to the event.
    Row_reader
      Extract the rows from the event.

  @section Delete_rows_log_event_binary_format Binary Format
*/
class Delete_rows_log_event : public Rows_log_event
{
public:
  enum 
  {
    /* Support interface to THD::binlog_prepare_pending_rows_event */
    TYPE_CODE = DELETE_ROWS_EVENT
  };

#ifdef MYSQL_SERVER
  Delete_rows_log_event(THD*, TABLE*, const Table_id&,
			bool is_transactional, const uchar* extra_row_info);
#endif
#ifdef HAVE_REPLICATION
  Delete_rows_log_event(const char *buf, uint event_len, 
			const Format_description_log_event *description_event);
#endif
#ifdef MYSQL_SERVER
  static bool binlog_row_logging_function(THD *thd, TABLE *table,
                                          bool is_transactional,
                                          const uchar *before_record,
                                          const uchar *after_record
                                          __attribute__((unused)))
  {
    return thd->binlog_delete_row(table, is_transactional,
                                  before_record, NULL);
  }
#endif
  
protected:
  virtual Log_event_type get_general_type_code() { return (Log_event_type)TYPE_CODE; }

#ifdef MYSQL_CLIENT
  void print(FILE *file, PRINT_EVENT_INFO *print_event_info);
#endif

#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_before_row_operations(const Slave_reporting_capability *const);
  virtual int do_after_row_operations(const Slave_reporting_capability *const,int);
  virtual int do_exec_row(const Relay_log_info *const);
#endif
};


#include "log_event_old.h"

/**
  @class Incident_log_event

   Class representing an incident, an occurance out of the ordinary,
   that happened on the master.

   The event is used to inform the slave that something out of the
   ordinary happened on the master that might cause the database to be
   in an inconsistent state.

   <table id="IncidentFormat">
   <caption>Incident event format</caption>
   <tr>
     <th>Symbol</th>
     <th>Format</th>
     <th>Description</th>
   </tr>
   <tr>
     <td>INCIDENT</td>
     <td align="right">2</td>
     <td>Incident number as an unsigned integer</td>
   </tr>
   <tr>
     <td>MSGLEN</td>
     <td align="right">1</td>
     <td>Message length as an unsigned integer</td>
   </tr>
   <tr>
     <td>MESSAGE</td>
     <td align="right">MSGLEN</td>
     <td>The message, if present. Not null terminated.</td>
   </tr>
   </table>

  @section Delete_rows_log_event_binary_format Binary Format
*/
class Incident_log_event : public Log_event {
public:
#ifdef MYSQL_SERVER
  Incident_log_event(THD *thd_arg, Incident incident)
    : Log_event(thd_arg, LOG_EVENT_NO_FILTER_F, Log_event::EVENT_NO_CACHE,
                Log_event::EVENT_IMMEDIATE_LOGGING), m_incident(incident)
  {
    DBUG_ENTER("Incident_log_event::Incident_log_event");
    DBUG_PRINT("enter", ("m_incident: %d", m_incident));
    m_message.str= NULL;                    /* Just as a precaution */
    m_message.length= 0;
    DBUG_VOID_RETURN;
  }

  Incident_log_event(THD *thd_arg, Incident incident, LEX_STRING const msg)
    : Log_event(thd_arg, LOG_EVENT_NO_FILTER_F,
                Log_event::EVENT_NO_CACHE,
                Log_event::EVENT_IMMEDIATE_LOGGING), m_incident(incident)
  {
    DBUG_ENTER("Incident_log_event::Incident_log_event");
    DBUG_PRINT("enter", ("m_incident: %d", m_incident));
    m_message.str= NULL;
    m_message.length= 0;
    if (!(m_message.str= (char*) my_malloc(key_memory_Incident_log_event_message,
                                           msg.length+1, MYF(MY_WME))))
    {
      /* Mark this event invalid */
      m_incident= INCIDENT_NONE;
      DBUG_VOID_RETURN;
    }
    strmake(m_message.str, msg.str, msg.length);
    m_message.length= msg.length;
    DBUG_VOID_RETURN;
  }
#endif

#ifdef MYSQL_SERVER
  int pack_info(Protocol*);
#endif

  Incident_log_event(const char *buf, uint event_len,
                     const Format_description_log_event *descr_event);

  virtual ~Incident_log_event();

#ifdef MYSQL_CLIENT
  virtual void print(FILE *file, PRINT_EVENT_INFO *print_event_info);
#endif

#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const *rli);
#endif

  virtual bool write_data_header(IO_CACHE *file);
  virtual bool write_data_body(IO_CACHE *file);

  virtual Log_event_type get_type_code() { return INCIDENT_EVENT; }

  virtual bool is_valid() const
  {
    return m_incident > INCIDENT_NONE && m_incident < INCIDENT_COUNT;
  }
  virtual int get_data_size() {
    return Binary_log_event::INCIDENT_HEADER_LEN + 1 + (uint) m_message.length;
  }

private:
  const char *description() const;

  Incident m_incident;
  LEX_STRING m_message;
};


/**
  @class Ignorable_log_event

  Base class for ignorable log events. Events deriving from
  this class can be safely ignored by slaves that cannot
  recognize them. Newer slaves, will be able to read and
  handle them. This has been designed to be an open-ended
  architecture, so adding new derived events shall not harm
  the old slaves that support ignorable log event mechanism
  (they will just ignore unrecognized ignorable events).

  @note The only thing that makes an event ignorable is that it has
  the LOG_EVENT_IGNORABLE_F flag set.  It is not strictly necessary
  that ignorable event types derive from Ignorable_log_event; they may
  just as well derive from Log_event and pass LOG_EVENT_IGNORABLE_F as
  argument to the Log_event constructor.
**/
class Ignorable_log_event : public Log_event {
public:
#ifndef MYSQL_CLIENT
  Ignorable_log_event(THD *thd_arg)
      : Log_event(thd_arg, LOG_EVENT_IGNORABLE_F, 
                  Log_event::EVENT_STMT_CACHE,
                  Log_event::EVENT_NORMAL_LOGGING)
  {
    DBUG_ENTER("Ignorable_log_event::Ignorable_log_event");
    DBUG_VOID_RETURN;
  }
#endif

  Ignorable_log_event(const char *buf,
                      const Format_description_log_event *descr_event);
  virtual ~Ignorable_log_event();

#ifndef MYSQL_CLIENT
  int pack_info(Protocol*);
#endif

#ifdef MYSQL_CLIENT
  virtual void print(FILE *file, PRINT_EVENT_INFO *print_event_info);
#endif

  virtual Log_event_type get_type_code() { return IGNORABLE_LOG_EVENT; }

  virtual bool is_valid() const { return 1; }

  virtual int get_data_size() { return Binary_log_event::IGNORABLE_HEADER_LEN; }
};


class Rows_query_log_event : public Ignorable_log_event {
public:
#ifndef MYSQL_CLIENT
  Rows_query_log_event(THD *thd_arg, const char * query, ulong query_len)
    : Ignorable_log_event(thd_arg)
  {
    DBUG_ENTER("Rows_query_log_event::Rows_query_log_event");
    if (!(m_rows_query= (char*) my_malloc(key_memory_Rows_query_log_event_rows_query,
                                          query_len + 1, MYF(MY_WME))))
      return;
    my_snprintf(m_rows_query, query_len + 1, "%s", query);
    DBUG_PRINT("enter", ("%s", m_rows_query));
    DBUG_VOID_RETURN;
  }
#endif

#ifndef MYSQL_CLIENT
  int pack_info(Protocol*);
#endif

  Rows_query_log_event(const char *buf, uint event_len,
                       const Format_description_log_event *descr_event);

  virtual ~Rows_query_log_event();

#ifdef MYSQL_CLIENT
  virtual void print(FILE *file, PRINT_EVENT_INFO *print_event_info);
#endif
  virtual bool write_data_body(IO_CACHE *file);

  virtual Log_event_type get_type_code() { return ROWS_QUERY_LOG_EVENT; }

  virtual int get_data_size()
  {
    return Binary_log_event::IGNORABLE_HEADER_LEN + 1 + (uint) strlen(m_rows_query);
  }
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const *rli);
#endif

private:

  char * m_rows_query;
};



static inline bool copy_event_cache_to_file_and_reinit(IO_CACHE *cache,
                                                       FILE *file,
                                                       bool flush_stream)
{
  return         
    my_b_copy_to_file(cache, file) ||
    (flush_stream ? (fflush(file) || ferror(file)) : 0) ||
    reinit_io_cache(cache, WRITE_CACHE, 0, FALSE, TRUE);
}

#ifdef MYSQL_SERVER
/*****************************************************************************

  Heartbeat Log Event class

  Replication event to ensure to slave that master is alive.
  The event is originated by master's dump thread and sent straight to
  slave without being logged. Slave itself does not store it in relay log
  but rather uses a data for immediate checks and throws away the event.

  Two members of the class log_ident and Log_event::log_pos comprise 
  @see the rpl_event_coordinates instance. The coordinates that a heartbeat
  instance carries correspond to the last event master has sent from
  its binlog.

 ****************************************************************************/
class Heartbeat_log_event: public Log_event
{
public:
  Heartbeat_log_event(const char* buf, uint event_len,
                      const Format_description_log_event* description_event);
  Log_event_type get_type_code() { return HEARTBEAT_LOG_EVENT; }
  bool is_valid() const
    {
      return (log_ident != NULL &&
              common_header->log_pos >= BIN_LOG_HEADER_SIZE);
    }
  const char * get_log_ident() { return log_ident; }
  uint get_ident_len() { return ident_len; }
  
private:
  const char* log_ident;
  uint ident_len;
};

/**
   The function is called by slave applier in case there are
   active table filtering rules to force gathering events associated
   with Query-log-event into an array to execute
   them once the fate of the Query is determined for execution.
*/
bool slave_execute_deferred_events(THD *thd);
#endif

int append_query_string(THD *thd, const CHARSET_INFO *csinfo,
                        String const *from, String *to);
extern TYPELIB binlog_checksum_typelib;

class Gtid_log_event : public Log_event
{
public:
  /*
    Prepare and commit sequence number. will be set to 0 if the event is not a
    transaction starter.
   */
  int64 commit_seq_no;
#ifndef MYSQL_CLIENT
  /**
    Create a new event using the GTID from the given Gtid_specification,
    or from @@SESSION.GTID_NEXT if spec==NULL.
  */
  Gtid_log_event(THD *thd_arg, bool using_trans,
                 const Gtid_specification *spec= NULL);
#endif

#ifndef MYSQL_CLIENT
  int pack_info(Protocol*);
#endif
  Gtid_log_event(const char *buffer, uint event_len,
                 const Format_description_log_event *descr_event);

  virtual ~Gtid_log_event() {}

  Log_event_type get_type_code()
  {
    DBUG_ENTER("Gtid_log_event::get_type_code()");
    Log_event_type ret= (spec.type == ANONYMOUS_GROUP ?
                         ANONYMOUS_GTID_LOG_EVENT : GTID_LOG_EVENT);
    DBUG_PRINT("info", ("code=%d=%s", ret, get_type_str(ret)));
    DBUG_RETURN(ret);
  }

  int get_data_size() { return POST_HEADER_LENGTH; }

private:
  /// Used internally by both print() and pack_info().
  size_t to_string(char *buf) const;

public:
#ifdef MYSQL_CLIENT
  void print(FILE *file, PRINT_EVENT_INFO *print_event_info);
#endif
#ifdef MYSQL_SERVER
  bool write_data_header(IO_CACHE *file);
#endif

#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  int do_apply_event(Relay_log_info const *rli);
  int do_update_pos(Relay_log_info *rli);
#endif

  /**
    Return the group type for this Gtid_log_event: this can be
    either ANONYMOUS_GROUP, AUTOMATIC_GROUP, or GTID_GROUP.
  */
  enum_group_type get_type() const { return spec.type; }
  bool is_valid() const { return true; }

  /**
    Return the SID for this GTID.  The SID is shared with the
    Log_event so it should not be modified.
  */
  const rpl_sid* get_sid() const { return &sid; }
  /**
    Return the SIDNO relative to the global sid_map for this GTID.

    This requires a lookup and possibly even update of global_sid_map,
    hence global_sid_lock must be held.  If global_sid_lock is not
    held, the caller must pass need_lock=true.  If there is an error
    (e.g. out of memory) while updating global_sid_map, this function
    returns a negative number.

    @param need_lock If true, the read lock on global_sid_lock is
    acquired and released inside this function; if false, the read
    lock or write lock must be held prior to calling this function.
    @retval SIDNO if successful
    @retval negative if adding SID to global_sid_map causes an error.
  */
  rpl_sidno get_sidno(bool need_lock)
  {
    if (spec.gtid.sidno < 0)
    {
      if (need_lock)
        global_sid_lock->rdlock();
      else
        global_sid_lock->assert_some_lock();
      spec.gtid.sidno= global_sid_map->add_sid(sid);
      if (need_lock)
        global_sid_lock->unlock();
    }
    return spec.gtid.sidno;
  }
  /**
    Return the SIDNO relative to the given Sid_map for this GTID.

    This assumes that the Sid_map is local to the thread, and thus
    does not use locks.

    @param sid_map The sid_map to use.
    @retval SIDNO if successful.
    @negative if adding SID to sid_map causes an error.
  */
  rpl_sidno get_sidno(Sid_map *sid_map)
  {
    return sid_map->add_sid(sid);
  }
  /// Return the GNO for this GTID.
  rpl_gno get_gno() const { return spec.gtid.gno; }
  /// Return true if this is the last group of the transaction, else false.
  bool get_commit_flag() const { return commit_flag; }

private:
  /// string holding the text "SET @@GLOBAL.GTID_NEXT = '"
  static const char *SET_STRING_PREFIX;
  /// Length of SET_STRING_PREFIX
  static const size_t SET_STRING_PREFIX_LENGTH= 26;
  /// The maximal length of the entire "SET ..." query.
  static const size_t MAX_SET_STRING_LENGTH= SET_STRING_PREFIX_LENGTH +
    rpl_sid::TEXT_LENGTH + 1 + MAX_GNO_TEXT_LENGTH + 1;

  /// Length of the commit_flag in event encoding
  static const int ENCODED_FLAG_LENGTH= 1;
  /// Length of SID in event encoding
  static const int ENCODED_SID_LENGTH= rpl_sid::BYTE_LENGTH;
  /// Length of GNO in event encoding
  static const int ENCODED_GNO_LENGTH= 8;
  /// Length of COMMIT TIMESTAMP index in event encoding
  static const int COMMIT_TS_INDEX_LEN= 1;

public:
  /// Total length of post header
  static const int POST_HEADER_LENGTH=
    ENCODED_FLAG_LENGTH      +  /* flags */
    ENCODED_SID_LENGTH       +  /* SID length */
    ENCODED_GNO_LENGTH       +  /* GNO length */
    COMMIT_TS_INDEX_LEN      +  /* TYPECODE for G_COMMIT_TS  */
    COMMIT_SEQ_LEN;             /* COMMIT sequence length */

private:
  /**
    Internal representation of the GTID.  The SIDNO will be
    uninitialized (value -1) until the first call to get_sidno(bool).
  */
  Gtid_specification spec;
  /// SID for this GTID.
  rpl_sid sid;
  /// True if this is the last group of the transaction, false otherwise.
  bool commit_flag;
};


class Previous_gtids_log_event : public Log_event
{
public:
#ifndef MYSQL_CLIENT
  Previous_gtids_log_event(const Gtid_set *set);
#endif

#ifndef MYSQL_CLIENT
  int pack_info(Protocol*);
#endif

  Previous_gtids_log_event(const char *buffer, uint event_len,
                           const Format_description_log_event *descr_event);
  virtual ~Previous_gtids_log_event() {}

  Log_event_type get_type_code() { return PREVIOUS_GTIDS_LOG_EVENT; }

  bool is_valid() const { return buf != NULL; }
  int get_data_size() { return buf_size; }

#ifdef MYSQL_CLIENT
  void print(FILE *file, PRINT_EVENT_INFO *print_event_info);
#endif
#ifdef MYSQL_SERVER
  bool write(IO_CACHE* file)
  {
    if (DBUG_EVALUATE_IF("skip_writing_previous_gtids_log_event", 1, 0))
    {
      DBUG_PRINT("info", ("skip writing Previous_gtids_log_event because of debug option 'skip_writing_previous_gtids_log_event'"));
      return false;
    }

    if (DBUG_EVALUATE_IF("write_partial_previous_gtids_log_event", 1, 0))
    {
      DBUG_PRINT("info", ("writing partial Previous_gtids_log_event because of debug option 'write_partial_previous_gtids_log_event'"));
      return (Log_event::write_header(file, get_data_size()) ||
              Log_event::write_data_header(file));
    }
  
    return (Log_event::write_header(file, get_data_size()) ||
            Log_event::write_data_header(file) ||
            write_data_body(file) ||
            Log_event::write_footer(file));
  }
  bool write_data_body(IO_CACHE *file);
#endif

  /// Return the encoded buffer, or NULL on error.
  const uchar *get_buf() { return buf; }
  /**
    Return the formatted string, or NULL on error.

    The string is allocated using my_malloc and it is the
    responsibility of the caller to free it.
  */
  char *get_str(size_t *length,
                const Gtid_set::String_format *string_format) const;
  /// Add all GTIDs from this event to the given Gtid_set.
  int add_to_set(Gtid_set *gtid_set) const;

#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  int do_apply_event(Relay_log_info const *rli) { return 0; }
  int do_update_pos(Relay_log_info *rli);
#endif

private:
  int buf_size;
  const uchar *buf;
};

inline bool is_gtid_event(Log_event* evt)
{
  return (evt->get_type_code() == GTID_LOG_EVENT ||
          evt->get_type_code() == ANONYMOUS_GTID_LOG_EVENT);
}

#ifdef MYSQL_SERVER
/*
  This is an utility function that adds a quoted identifier into the a buffer.
  This also escapes any existance of the quote string inside the identifier.
 */
size_t my_strmov_quoted_identifier(THD *thd, char *buffer,
                                   const char* identifier,
                                   uint length);
#else
size_t my_strmov_quoted_identifier(char *buffer, const char* identifier);
#endif
size_t my_strmov_quoted_identifier_helper(int q, char *buffer,
                                          const char* identifier,
                                          uint length);

/**
  @} (end of group Replication)
*/

#endif /* _log_event_h */

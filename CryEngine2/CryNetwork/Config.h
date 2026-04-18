#ifndef __NET_CONFIG_H__
#define __NET_CONFIG_H__

#pragma once

// enable a good toolkit of debug stuff for when you're developing code
#define USUAL_DEBUG_STUFF 0

#define USE_GAMESPY_SDK 0
#define USE_DEFENCE 1
#define ALWAYS_CHECK_CD_KEYS 1

/*
 * PROJECT WIDE CONSTANTS
 */
#define UDP_HEADER_SIZE 28
// we make this somewhat smaller than spec because we don't expect to ever see packets this large!
//#define MAX_UDP_PACKET_SIZE 65535
#define MAX_UDP_PACKET_SIZE 4096

static const float WORLDSIZE_X = 8192.0f;
static const float WORLDSIZE_Y = 8192.0f;
static const float WORLDSIZE_Z = 4096.0f;
static const int WORLDPOSBITS_X = 24;
static const int WORLDPOSBITS_Y = 24;
static const int WORLDPOSBITS_Z = 24;
static const int WORLDPOS_COMPONENTMAXBITS = 20;
static const float WORLDPOS_ERRORFRAC = 0.03f;

/*
 * PROJECT WIDE CONFIGURATION
 */

#define ENABLE_DEBUG_KIT 0

// debug the net timer
#define TIMER_DEBUG 0

#define LOG_ENCODING_TO_FILE 0

#define DETAIL_REGULARLY_SYNCED_ITEM_DEBUG 0

#define ENABLE_ASPECT_HASHING 0


#define CRC8_ASPECT_FORMAT 1
#define CRC8_ENCODING_GLOBAL 1
#define CRC8_ENCODING_MESSAGE 1

#define MMM_CHECK_LEAKS 0

// write the name of each property into the stream, to assist in finding
// encoders that fail
#define CHECK_ENCODING 0
#define TRACK_ENCODING 0
// write a single bit before & after each property (similar to above check)
// sometimes finds bugs that the above cannot
#define MINI_CHECK_ENCODING 1

// debug machinery to ensure that message headers are encoded correctly (must be turned off for retail)
#define DOUBLE_WRITE_MESSAGE_HEADERS 1

// enable tracking the last N messages sent & received (or remove the capability to do so if == 0)
#define MESSAGE_BACKTRACE_LENGTH 20

#define CHECK_MEMENTO_AGES 1

#define LOAD_NETWORK_CODE 0
#define LOAD_NETWORK_PKTSIZE 32
#define LOAD_NETWORK_COUNT 1024

#define VERIFY_MEMENTO_BUFFERS 0

// enable this to check that buffers don't change between receiving a packet,
// and finishing processing it (requires turning off ALLOW_ENCRYPTION)
#define ENABLE_BUFFER_VERIFICATION 0

// enable this to create a much fatter stream that is useful for
// debugging whether compression is working (or not)
#define DEBUG_STREAM_INTEGRITY 0

// enable this to get a warning whenever a value is clamped by quantization
#define DEBUG_QUANTIZATION_CLAMPING 0

// enable 'accurate' (sub-bit estimation) bandwidth profiling
#define ENABLE_ACCURATE_BANDWIDTH_PROFILING 0

// enable this to start debugging that sequence numbers are not
// being upset
#define DEBUG_SEQUENCE_NUMBERS 0

#define DEBUG_TIME_COMPRESSION 0

// enable this to ensure that no duplicate sequence numbers are acked/nacked
#define DETECT_DUPLICATE_ACKS 0

// enable this to write input and output state logs for the connection
// (for debugging logic in the endpoint)
#define DEBUG_ENDPOINT_LOGIC 0

// enable this to write equation based bit rate to a log file per ctpendpoint
#define LOG_TFRC 0

// enable a small check that synchronizations have the same basis/non-basis values
#define CHECK_SYNC_BASIS 0

// enable this for debug logging to be available (controlled:)
#define DEBUG_LOGGING 0

#define VERBOSE_MALFORMED_PACKET_REPORTS 1

// enable this to allow encryption
#define ALLOW_ENCRYPTION 1

// never ever release with this defined
#define INTERNET_SIMULATOR 1

#define USE_SYSTEM_ALLOCATOR 0

// also never release with these defined
#define STATS_COLLECTOR_FILE 0
#ifndef XENON
#define STATS_COLLECTOR_DEBUG_KIT 0 // safe to leave this on... not enabling debug kit will turn it off
#define STATS_COLLECTOR_INTERACTIVE 0
#else
#define STATS_COLLECTOR_DEBUG_KIT 0 // safe to leave this on... not enabling debug kit will turn it off
#define STATS_COLLECTOR_INTERACTIVE 0
#endif

#define INCLUDE_DEMO_RECORDING 1

// should the endpoint log incoming or outgoing messages - enable with net_logmessages
#define LOG_INCOMING_MESSAGES 1
#define LOG_OUTGOING_MESSAGES 1
// logging capability for when buffers get updated - enable with net_logbuffers
#define LOG_BUFFER_UPDATES 0

#define LOG_NETCONTEXT_MESSAGES 0

// these switches will cause a file to be written containing the raw data
// that was read or written each block (= LOTS of files, but useful for
// debugging) -- for compressing stream
#define COMPRESSING_STREAM_DEBUG_WHAT_GETS_WRITTEN 0
#define COMPRESSING_STREAM_DEBUG_WHAT_GETS_READ    0
#define COMPRESSING_STREAM_SANITY_CHECK_EVERYTHING 0
#define COMPRESSING_STREAM_USE_BURROWS_WHEELER     0

// enable some debug logs for profile switches
#define ENABLE_PROFILE_DEBUGGING 0

// this switch enables tracking session id's
// it also enables sending them to an internal server
#define ENABLE_SESSION_IDS 0

//Entity id 0000028f is not known in the network system - probably has not been bound
//Entity id 0000028f is not known in the network system - probably has not been bound
//Entity id 0000028f is not known in the network system - probably has not been bound
#define ENABLE_UNBOUND_ENTITY_DEBUGGING 0

#define ENABLE_DISTRIBUTED_LOGGER 0

// log every time a lock gets taken by the place it's taken from
#define LOG_LOCKING 0

// insert checks into CPolymorphicQueue to ensure consistency
#define CHECKING_POLYMORPHIC_QUEUE 0

// for checking very special sequences of problems
#define FORCE_DECODING_TO_GAME_THREAD 0

// insert checks for who's binding/unbinding objects
#define CHECKING_BIND_REFCOUNTS 0

#define LOG_BREAKABILITY 1
#define LOG_ENTITYID_ERRORS 1
#define LOG_MESSAGE_DROPS 1

// 

/*
 * from here on is validation of the above
 */

#if CRYNETWORK_RELEASEBUILD

# undef USUAL_DEBUG_STUFF
# undef USE_DEFENCE
# undef ENABLE_DEBUG_KIT
# undef TIMER_DEBUG
# undef LOG_ENCODING_TO_FILE
# undef DETAIL_REGULARLY_SYNCED_ITEM_DEBUG
# undef CRC8_ASPECT_FORMAT
# undef CRC8_ENCODING_GLOBAL
# undef CRC8_ENCODING_MESSAGE
# undef MMM_CHECK_LEAKS
# undef CHECK_ENCODING
# undef TRACK_ENCODING
# undef MINI_CHECK_ENCODING
# undef DOUBLE_WRITE_MESSAGE_HEADERS
# undef MESSAGE_BACKTRACE_LENGTH
# undef CHECK_MEMENTO_AGES
# undef LOAD_NETWORK_CODE
# undef VERIFY_MEMENTO_BUFFERS
# undef ENABLE_BUFFER_VERIFICATION
# undef DEBUG_STREAM_INTEGRITY
# undef DEBUG_QUANTIZATION_CLAMPING
# undef ENABLE_ACCURATE_BANDWIDTH_PROFILING
# undef DEBUG_SEQUENCE_NUMBERS
# undef DEBUG_TIME_COMPRESSION
# undef DETECT_DUPLICATE_ACKS
# undef DEBUG_ENDPOINT_LOGIC
# undef LOG_TFRC
# undef CHECK_SYNC_BASIS
# undef DEBUG_LOGGING
# undef VERBOSE_MALFORMED_PACKET_REPORTS
# undef INTERNET_SIMULATOR
# undef USE_SYSTEM_ALLOCATOR
# undef STATS_COLLECTOR_FILE
# undef STATS_COLLECTOR_DEBUG_KIT
# undef STATS_COLLECTOR_INTERACTIVE
# undef LOG_INCOMING_MESSAGES
# undef LOG_OUTGOING_MESSAGES
# undef LOG_BUFFER_UPDATES
# undef LOG_NETCONTEXT_MESSAGES
# undef COMPRESSING_STREAM_DEBUG_WHAT_GETS_WRITTEN
# undef COMPRESSING_STREAM_DEBUG_WHAT_GETS_READ
# undef COMPRESSING_STREAM_SANITY_CHECK_EVERYTHING
# undef ENABLE_PROFILE_DEBUGGING
# undef ENABLE_SESSION_IDS
# undef ENABLE_UNBOUND_ENTITY_DEBUGGING
# undef ENABLE_DISTRIBUTED_LOGGER
# undef LOG_LOCKING
# undef CHECKING_POLYMORPHIC_QUEUE
# undef FORCE_DECODING_TO_GAME_THREAD
# undef CHECKING_BIND_REFCOUNTS
# undef LOG_BREAKABILITY
# undef LOG_ENTITYID_ERRORS
# undef LOG_MESSAGE_DROPS

# define USUAL_DEBUG_STUFF 0
# define USE_DEFENCE 1
# define ENABLE_DEBUG_KIT 0
# define TIMER_DEBUG 0
# define LOG_ENCODING_TO_FILE 0
# define DETAIL_REGULARLY_SYNCED_ITEM_DEBUG 0
# define CRC8_ASPECT_FORMAT 0
# define CRC8_ENCODING_GLOBAL 0
# define CRC8_ENCODING_MESSAGE 0
# define MMM_CHECK_LEAKS 0
# define CHECK_ENCODING 0
# define TRACK_ENCODING 0
# define MINI_CHECK_ENCODING 0
# define DOUBLE_WRITE_MESSAGE_HEADERS 0
# define MESSAGE_BACKTRACE_LENGTH 0
# define CHECK_MEMENTO_AGES 0
# define LOAD_NETWORK_CODE 0
# define VERIFY_MEMENTO_BUFFERS 0
# define ENABLE_BUFFER_VERIFICATION 0
# define DEBUG_STREAM_INTEGRITY 0
# define DEBUG_QUANTIZATION_CLAMPING 0
# define ENABLE_ACCURATE_BANDWIDTH_PROFILING 0
# define DEBUG_SEQUENCE_NUMBERS 0
# define DEBUG_TIME_COMPRESSION 0
# define DETECT_DUPLICATE_ACKS 0
# define DEBUG_ENDPOINT_LOGIC 0
# define LOG_TFRC 0
# define CHECK_SYNC_BASIS 0
# define DEBUG_LOGGING 0
# define VERBOSE_MALFORMED_PACKET_REPORTS 0
# define INTERNET_SIMULATOR 0
# define USE_SYSTEM_ALLOCATOR 0
# define STATS_COLLECTOR_FILE 0
# define STATS_COLLECTOR_DEBUG_KIT 0
# define STATS_COLLECTOR_INTERACTIVE 0
# define LOG_INCOMING_MESSAGES 0
# define LOG_OUTGOING_MESSAGES 0
# define LOG_BUFFER_UPDATES 0
# define LOG_NETCONTEXT_MESSAGES 0
# define COMPRESSING_STREAM_DEBUG_WHAT_GETS_WRITTEN 0
# define COMPRESSING_STREAM_DEBUG_WHAT_GETS_READ 0
# define COMPRESSING_STREAM_SANITY_CHECK_EVERYTHING 0
# define ENABLE_PROFILE_DEBUGGING 0
# define ENABLE_SESSION_IDS 0
# define ENABLE_UNBOUND_ENTITY_DEBUGGING 0
# define ENABLE_DISTRIBUTED_LOGGER 0
# define LOG_LOCKING 0
# define CHECKING_POLYMORPHIC_QUEUE 0
# define FORCE_DECODING_TO_GAME_THREAD 0
# define CHECKING_BIND_REFCOUNTS 0
# define LOG_BREAKABILITY 0
# define LOG_ENTITYID_ERRORS 0
# define LOG_MESSAGE_DROPS 0

#endif

#ifdef NDEBUG
# undef CHECKING_POLYMORPHIC_QUEUE
# define CHECKING_POLYMORPHIC_QUEUE 0
#endif

#if LOG_ENCODING_TO_FILE
# undef ENABLE_DEBUG_KIT
# define ENABLE_DEBUG_KIT 1
#endif

#if USUAL_DEBUG_STUFF
# undef CHECK_ENCODING
# define CHECK_ENCODING 1
# undef DEBUG_STREAM_INTEGRITY
# define DEBUG_STREAM_INTEGRITY 1
# undef CHECK_SYNC_BASIS
# define CHECK_SYNC_BASIS 1
# undef ENABLE_SESSION_IDS
# define ENABLE_SESSION_IDS 1
# undef CHECK_MEMENTO_AGES
# define CHECK_MEMENTO_AGES 1
#endif

#if CHECK_ENCODING
# if MINI_CHECK_ENCODING
#  error Cannot define CHECK_ENCODING and MINI_CHECK_ENCODING at once
# endif
#endif

#if ALLOW_ENCRYPTION
# if ENABLE_BUFFER_VERIFICATION
#  error Cannot define ENABLE_BUFFER_VERIFICATION and ALLOW_ENCRYPTION at once
# endif
#endif

#if INTERNET_SIMULATOR
# if _MSC_VER > 1000
#  pragma message ("Internet Simulator is turned on: do not release this binary")
# endif
#endif

#if ENABLE_DEBUG_KIT && !ENABLE_SESSION_IDS
# undef ENABLE_SESSION_IDS
# define ENABLE_SESSION_IDS 1
# if _MSC_VER > 1000
#  pragma message ("Session debugging is turned on: do not release this binary")
# endif
#endif

#if !ENABLE_DEBUG_KIT
# undef STATS_COLLECTOR_DEBUG_KIT
# define STATS_COLLECTOR_DEBUG_KIT 0
#endif

#define STATS_COLLECTOR (STATS_COLLECTOR_DEBUG_KIT || STATS_COLLECTOR_FILE || STATS_COLLECTOR_INTERACTIVE)

#if STATS_COLLECTOR
# undef TRACK_ENCODING
# define TRACK_ENCODING 1
#endif

#if CHECK_ENCODING
# undef TRACK_ENCODING
# define TRACK_ENCODING 1
#endif

#if STATS_COLLECTOR
# if _MSC_VER > 1000
#  pragma message ("Stats collection is turned on: do not release this binary")
# endif
#endif

#if USE_GAMESPY_SDK
# ifdef XENON
#  pragma message ("No gamespy on xenon")
#  undef USE_GAMESPY_SDK
#  define USE_GAMESPY_SDK 0
# endif
# ifdef PS3
#  pragma message ("No gamespy on PS3")
#  undef USE_GAMESPY_SDK
#  define USE_GAMESPY_SDK 0
# endif
# ifdef LINUX
#  pragma message ("No gamespy on Linux")
#  undef USE_GAMESPY_SDK
#  define USE_GAMESPY_SDK 0
# endif
#endif

#if ENABLE_DEBUG_KIT && (MESSAGE_BACKTRACE_LENGTH < 100)
# undef MESSAGE_BACKTRACE_LENGTH
# define MESSAGE_BACKTRACE_LENGTH 100
#endif

#define CRC8_ENCODING (CRC8_ENCODING_ASPECT || CRC8_ENCODING_GLOBAL)

#endif

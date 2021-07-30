#ifndef TINYVPN_SSL_CLIENT2_H
#define TINYVPN_SSL_CLIENT2_H

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#include <stdio.h>
#include <stdlib.h>
#define mbedtls_time       time
#define mbedtls_time_t     time_t
#define mbedtls_printf     printf
#define mbedtls_fprintf    fprintf
#define mbedtls_snprintf   snprintf
#define mbedtls_exit            exit
#define MBEDTLS_EXIT_SUCCESS    EXIT_SUCCESS
#define MBEDTLS_EXIT_FAILURE    EXIT_FAILURE
#endif

#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/certs.h"
#include "mbedtls/x509.h"
#include "mbedtls/error.h"
#include "mbedtls/debug.h"
#include "mbedtls/timing.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#define MAX_REQUEST_SIZE      20000
#define MAX_REQUEST_SIZE_STR "20000"

#define DFL_SERVER_NAME         "localhost"
#define DFL_SERVER_ADDR         NULL
#define DFL_SERVER_PORT         "4433"
#define DFL_REQUEST_PAGE        "/"
#define DFL_REQUEST_SIZE        -1
#define DFL_DEBUG_LEVEL         0
#define DFL_NBIO                0
#define DFL_EVENT               0
#define DFL_READ_TIMEOUT        0
#define DFL_MAX_RESEND          0
#define DFL_CA_FILE             ""
#define DFL_CA_PATH             ""
#define DFL_CRT_FILE            ""
#define DFL_KEY_FILE            ""
#define DFL_PSK                 ""
#define DFL_PSK_IDENTITY        "Client_identity"
#define DFL_ECJPAKE_PW          NULL
#define DFL_EC_MAX_OPS          -1
#define DFL_FORCE_CIPHER        0
#define DFL_RENEGOTIATION       MBEDTLS_SSL_RENEGOTIATION_DISABLED
#define DFL_ALLOW_LEGACY        -2
#define DFL_RENEGOTIATE         0
#define DFL_EXCHANGES           1
#define DFL_MIN_VERSION         -1
#define DFL_MAX_VERSION         -1
#define DFL_ARC4                -1
#define DFL_SHA1                -1
#define DFL_AUTH_MODE           -1
#define DFL_MFL_CODE            MBEDTLS_SSL_MAX_FRAG_LEN_NONE
#define DFL_TRUNC_HMAC          -1
#define DFL_RECSPLIT            -1
#define DFL_DHMLEN              -1
#define DFL_RECONNECT           0
#define DFL_RECO_DELAY          0
#define DFL_RECONNECT_HARD      0
#define DFL_TICKETS             MBEDTLS_SSL_SESSION_TICKETS_ENABLED
#define DFL_ALPN_STRING         NULL
#define DFL_CURVES              NULL
#define DFL_TRANSPORT           MBEDTLS_SSL_TRANSPORT_STREAM
#define DFL_HS_TO_MIN           0
#define DFL_HS_TO_MAX           0
#define DFL_DTLS_MTU            -1
#define DFL_DGRAM_PACKING        1
#define DFL_FALLBACK            -1
#define DFL_EXTENDED_MS         -1
#define DFL_ETM                 -1
#define DFL_SKIP_CLOSE_NOTIFY   0

#define GET_REQUEST "GET %s HTTP/1.0\r\nExtra-header: "
#define GET_REQUEST_END "\r\n\r\n"

#if defined(MBEDTLS_X509_CRT_PARSE_C)
#if defined(MBEDTLS_FS_IO)
#define USAGE_IO \
    "    ca_file=%%s          The single file containing the top-level CA(s) you fully trust\n" \
    "                        default: \"\" (pre-loaded)\n" \
    "                        use \"none\" to skip loading any top-level CAs.\n" \
    "    ca_path=%%s          The path containing the top-level CA(s) you fully trust\n" \
    "                        default: \"\" (pre-loaded) (overrides ca_file)\n" \
    "                        use \"none\" to skip loading any top-level CAs.\n" \
    "    crt_file=%%s         Your own cert and chain (in bottom to top order, top may be omitted)\n" \
    "                        default: \"\" (pre-loaded)\n" \
    "    key_file=%%s         default: \"\" (pre-loaded)\n"
#else
#define USAGE_IO \
    "    No file operations available (MBEDTLS_FS_IO not defined)\n"
#endif /* MBEDTLS_FS_IO */
#else
#define USAGE_IO ""
#endif /* MBEDTLS_X509_CRT_PARSE_C */

#if defined(MBEDTLS_KEY_EXCHANGE__SOME__PSK_ENABLED)
#define USAGE_PSK                                                   \
    "    psk=%%s              default: \"\" (in hex, without 0x)\n" \
    "    psk_identity=%%s     default: \"Client_identity\"\n"
#else
#define USAGE_PSK ""
#endif /* MBEDTLS_KEY_EXCHANGE__SOME__PSK_ENABLED */

#if defined(MBEDTLS_SSL_SESSION_TICKETS)
#define USAGE_TICKETS                                       \
    "    tickets=%%d          default: 1 (enabled)\n"
#else
#define USAGE_TICKETS ""
#endif /* MBEDTLS_SSL_SESSION_TICKETS */

#if defined(MBEDTLS_SSL_TRUNCATED_HMAC)
#define USAGE_TRUNC_HMAC                                    \
    "    trunc_hmac=%%d       default: library default\n"
#else
#define USAGE_TRUNC_HMAC ""
#endif /* MBEDTLS_SSL_TRUNCATED_HMAC */

#if defined(MBEDTLS_SSL_MAX_FRAGMENT_LENGTH)
#define USAGE_MAX_FRAG_LEN                                      \
    "    max_frag_len=%%d     default: 16384 (tls default)\n"   \
    "                        options: 512, 1024, 2048, 4096\n"
#else
#define USAGE_MAX_FRAG_LEN ""
#endif /* MBEDTLS_SSL_MAX_FRAGMENT_LENGTH */

#if defined(MBEDTLS_SSL_CBC_RECORD_SPLITTING)
#define USAGE_RECSPLIT \
    "    recsplit=0/1        default: (library default: on)\n"
#else
#define USAGE_RECSPLIT
#endif

#if defined(MBEDTLS_DHM_C)
#define USAGE_DHMLEN \
    "    dhmlen=%%d           default: (library default: 1024 bits)\n"
#else
#define USAGE_DHMLEN
#endif

#if defined(MBEDTLS_SSL_ALPN)
#define USAGE_ALPN \
    "    alpn=%%s             default: \"\" (disabled)\n"   \
    "                        example: spdy/1,http/1.1\n"
#else
#define USAGE_ALPN ""
#endif /* MBEDTLS_SSL_ALPN */

#if defined(MBEDTLS_ECP_C)
#define USAGE_CURVES \
    "    curves=a,b,c,d      default: \"default\" (library default)\n"  \
    "                        example: \"secp521r1,brainpoolP512r1\"\n"  \
    "                        - use \"none\" for empty list\n"           \
    "                        - see mbedtls_ecp_curve_list()\n"          \
    "                          for acceptable curve names\n"
#else
#define USAGE_CURVES ""
#endif

#if defined(MBEDTLS_SSL_PROTO_DTLS)
#define USAGE_DTLS \
    "    dtls=%%d             default: 0 (TLS)\n"                           \
    "    hs_timeout=%%d-%%d    default: (library default: 1000-60000)\n"    \
    "                        range of DTLS handshake timeouts in millisecs\n" \
    "    mtu=%%d              default: (library default: unlimited)\n"  \
    "    dgram_packing=%%d    default: 1 (allowed)\n"                   \
    "                        allow or forbid packing of multiple\n" \
    "                        records within a single datgram.\n"
#else
#define USAGE_DTLS ""
#endif

#if defined(MBEDTLS_SSL_FALLBACK_SCSV)
#define USAGE_FALLBACK \
    "    fallback=0/1        default: (library default: off)\n"
#else
#define USAGE_FALLBACK ""
#endif

#if defined(MBEDTLS_SSL_EXTENDED_MASTER_SECRET)
#define USAGE_EMS \
    "    extended_ms=0/1     default: (library default: on)\n"
#else
#define USAGE_EMS ""
#endif

#if defined(MBEDTLS_SSL_ENCRYPT_THEN_MAC)
#define USAGE_ETM \
    "    etm=0/1             default: (library default: on)\n"
#else
#define USAGE_ETM ""
#endif

#if defined(MBEDTLS_SSL_RENEGOTIATION)
#define USAGE_RENEGO \
    "    renegotiation=%%d    default: 0 (disabled)\n"      \
    "    renegotiate=%%d      default: 0 (disabled)\n"
#else
#define USAGE_RENEGO ""
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED)
#define USAGE_ECJPAKE \
    "    ecjpake_pw=%%s       default: none (disabled)\n"
#else
#define USAGE_ECJPAKE ""
#endif

#if defined(MBEDTLS_ECP_RESTARTABLE)
#define USAGE_ECRESTART \
    "    ec_max_ops=%%s       default: library default (restart disabled)\n"
#else
#define USAGE_ECRESTART ""
#endif

#define USAGE \
    "\n usage: ssl_client2 param=<>...\n"                   \
    "\n acceptable parameters:\n"                           \
    "    server_name=%%s      default: localhost\n"         \
    "    server_addr=%%s      default: given by name\n"     \
    "    server_port=%%d      default: 4433\n"              \
    "    request_page=%%s     default: \".\"\n"             \
    "    request_size=%%d     default: about 34 (basic request)\n"           \
    "                        (minimum: 0, max: " MAX_REQUEST_SIZE_STR ")\n"  \
    "                        If 0, in the first exchange only an empty\n"    \
    "                        application data message is sent followed by\n" \
    "                        a second non-empty message before attempting\n" \
    "                        to read a response from the server\n"           \
    "    debug_level=%%d      default: 0 (disabled)\n"             \
    "    nbio=%%d             default: 0 (blocking I/O)\n"         \
    "                        options: 1 (non-blocking), 2 (added delays)\n"   \
    "    event=%%d            default: 0 (loop)\n"                            \
    "                        options: 1 (level-triggered, implies nbio=1),\n" \
    "    read_timeout=%%d     default: 0 ms (no timeout)\n"        \
    "    max_resend=%%d       default: 0 (no resend on timeout)\n" \
    "    skip_close_notify=%%d default: 0 (send close_notify)\n" \
    "\n"                                                    \
    USAGE_DTLS                                              \
    "\n"                                                    \
    "    auth_mode=%%s        default: (library default: none)\n" \
    "                        options: none, optional, required\n" \
    USAGE_IO                                                \
    "\n"                                                    \
    USAGE_PSK                                               \
    USAGE_ECJPAKE                                           \
    USAGE_ECRESTART                                         \
    "\n"                                                    \
    "    allow_legacy=%%d     default: (library default: no)\n"   \
    USAGE_RENEGO                                            \
    "    exchanges=%%d        default: 1\n"                 \
    "    reconnect=%%d        default: 0 (disabled)\n"      \
    "    reco_delay=%%d       default: 0 seconds\n"         \
    "    reconnect_hard=%%d   default: 0 (disabled)\n"      \
    USAGE_TICKETS                                           \
    USAGE_MAX_FRAG_LEN                                      \
    USAGE_TRUNC_HMAC                                        \
    USAGE_ALPN                                              \
    USAGE_FALLBACK                                          \
    USAGE_EMS                                               \
    USAGE_ETM                                               \
    USAGE_CURVES                                            \
    USAGE_RECSPLIT                                          \
    USAGE_DHMLEN                                            \
    "\n"                                                    \
    "    arc4=%%d             default: (library default: 0)\n" \
    "    allow_sha1=%%d       default: 0\n"                             \
    "    min_version=%%s      default: (library default: tls1)\n"       \
    "    max_version=%%s      default: (library default: tls1_2)\n"     \
    "    force_version=%%s    default: \"\" (none)\n"       \
    "                        options: ssl3, tls1, tls1_1, tls1_2, dtls1, dtls1_2\n" \
    "\n"                                                    \
    "    force_ciphersuite=<name>    default: all enabled\n"\
    "    query_config=<name>         return 0 if the specified\n"       \
    "                                configuration macro is defined and 1\n"  \
    "                                otherwise. The expansion of the macro\n" \
    "                                is printed if it is defined\n"     \
    " acceptable ciphersuite names:\n"

#define ALPN_LIST_SIZE  10
#define CURVE_LIST_SIZE 20


/*
 * global options
 */
struct options
{
    const char *server_name;    /* hostname of the server (client only)     */
    const char *server_addr;    /* address of the server (client only)      */
    const char *server_port;    /* port on which the ssl service runs       */
    int debug_level;            /* level of debugging                       */
    int nbio;                   /* should I/O be blocking?                  */
    int event;                  /* loop or event-driven IO? level or edge triggered? */
    uint32_t read_timeout;      /* timeout on mbedtls_ssl_read() in milliseconds     */
    int max_resend;             /* DTLS times to resend on read timeout     */
    const char *request_page;   /* page on server to request                */
    int request_size;           /* pad request with header to requested size */
    const char *ca_file;        /* the file with the CA certificate(s)      */
    const char *ca_path;        /* the path with the CA certificate(s) reside */
    const char *crt_file;       /* the file with the client certificate     */
    const char *key_file;       /* the file with the client key             */
    const char *psk;            /* the pre-shared key                       */
    const char *psk_identity;   /* the pre-shared key identity              */
    const char *ecjpake_pw;     /* the EC J-PAKE password                   */
    int ec_max_ops;             /* EC consecutive operations limit          */
    int force_ciphersuite[2];   /* protocol/ciphersuite to use, or all      */
    int renegotiation;          /* enable / disable renegotiation           */
    int allow_legacy;           /* allow legacy renegotiation               */
    int renegotiate;            /* attempt renegotiation?                   */
    int renego_delay;           /* delay before enforcing renegotiation     */
    int exchanges;              /* number of data exchanges                 */
    int min_version;            /* minimum protocol version accepted        */
    int max_version;            /* maximum protocol version accepted        */
    int arc4;                   /* flag for arc4 suites support             */
    int allow_sha1;             /* flag for SHA-1 support                   */
    int auth_mode;              /* verify mode for connection               */
    unsigned char mfl_code;     /* code for maximum fragment length         */
    int trunc_hmac;             /* negotiate truncated hmac or not          */
    int recsplit;               /* enable record splitting?                 */
    int dhmlen;                 /* minimum DHM params len in bits           */
    int reconnect;              /* attempt to resume session                */
    int reco_delay;             /* delay in seconds before resuming session */
    int reconnect_hard;         /* unexpectedly reconnect from the same port */
    int tickets;                /* enable / disable session tickets         */
    const char *curves;         /* list of supported elliptic curves        */
    const char *alpn_string;    /* ALPN supported protocols                 */
    int transport;              /* TLS or DTLS?                             */
    uint32_t hs_to_min;         /* Initial value of DTLS handshake timer    */
    uint32_t hs_to_max;         /* Max value of DTLS handshake timer        */
    int dtls_mtu;               /* UDP Maximum tranport unit for DTLS       */
    int fallback;               /* is this a fallback connection?           */
    int dgram_packing;          /* allow/forbid datagram packing            */
    int extended_ms;            /* negotiate extended master secret?        */
    int etm;                    /* negotiate encrypt then mac?              */
    int skip_close_notify;      /* skip sending the close_notify alert      */
} ;

int init_ssl_client( );
int connect_ssl(std::string& ip, uint16_t port, int& sock);
int get_private_ip(int premium, std::string& androidId, std::string& userName, std::string& userPassword, std::string& recv_data);
int ssl_read(char* ip_packet_data, int& ip_packet_len);
int ssl_write(uint8_t* buf, int len);
int ssl_close();
#endif //TINYVPN_SSL_CLIENT2_H

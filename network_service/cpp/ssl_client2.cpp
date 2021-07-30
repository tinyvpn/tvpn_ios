#include "ssl_client2.h"
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <os/log.h>
#include "sysutl.h"
#include "stringutl.h"
//#include "native-lib.h"
int query_config( const char *config );

static void my_debug( void *ctx, int level,
                      const char *file, int line,
                      const char *str )
{
    const char *p, *basename;

    /* Extract basename from file */
    for( p = basename = file; *p != '\0'; p++ )
        if( *p == '/' || *p == '\\' )
            basename = p + 1;

    mbedtls_fprintf( (FILE *) ctx, "%s:%04d: |%d| %s",
                     basename, line, level, str );
    fflush(  (FILE *) ctx  );
}

/*
 * Enabled if debug_level > 1 in code below
 */
static int my_verify( void *data, mbedtls_x509_crt *crt,
                      int depth, uint32_t *flags )
{
    char buf[1024];
    ((void) data);

    mbedtls_printf( "\nVerify requested for (Depth %d):\n", depth );
    mbedtls_x509_crt_info( buf, sizeof( buf ) - 1, "", crt );
    mbedtls_printf( "%s", buf );

    if ( ( *flags ) == 0 )
        mbedtls_printf( "  This certificate has no flags\n" );
    else
    {
        mbedtls_x509_crt_verify_info( buf, sizeof( buf ), "  ! ", *flags );
        mbedtls_printf( "%s\n", buf );
    }

    return( 0 );
}

static int ssl_sig_hashes_for_test[] = {
    MBEDTLS_MD_SHA512,
    MBEDTLS_MD_SHA384,
    MBEDTLS_MD_SHA256,
    MBEDTLS_MD_SHA224,
    /* Allow SHA-1 as we use it extensively in tests. */
    MBEDTLS_MD_SHA1,
    MBEDTLS_MD_NONE
};

struct options opt;

mbedtls_net_context server_fd;
const char *pers = "ssl_client2";
mbedtls_x509_crt_profile crt_profile_for_test = mbedtls_x509_crt_profile_default;
mbedtls_entropy_context entropy;
mbedtls_ctr_drbg_context ctr_drbg;
mbedtls_ssl_context ssl;
mbedtls_ssl_config conf;
mbedtls_ssl_session saved_session;
mbedtls_timing_delay_context timer;
uint32_t flags;
mbedtls_x509_crt cacert;
mbedtls_x509_crt clicert;
mbedtls_pk_context pkey;

int init_ssl_client( ) {
    int ret = 0; //, len, tail_len, written, frags, retry_left;
    int i;
    char *p, *q;
    const int *list;
    /*
     * Make sure memory references are valid.
     */
    mbedtls_net_init(&server_fd);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    memset(&saved_session, 0, sizeof(mbedtls_ssl_session));
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_x509_crt_init(&cacert);
    mbedtls_x509_crt_init(&clicert);
    mbedtls_pk_init(&pkey);

    opt.server_name = DFL_SERVER_NAME;
    //opt.server_addr = DFL_SERVER_ADDR;
    //opt.server_port = DFL_SERVER_PORT;
    opt.debug_level = DFL_DEBUG_LEVEL;
    opt.nbio = DFL_NBIO;
    opt.event = DFL_EVENT;
    opt.read_timeout = DFL_READ_TIMEOUT;
    opt.max_resend = DFL_MAX_RESEND;
    opt.request_page = DFL_REQUEST_PAGE;
    opt.request_size = DFL_REQUEST_SIZE;
    opt.ca_file = DFL_CA_FILE;
    opt.ca_path = DFL_CA_PATH;
    opt.crt_file = DFL_CRT_FILE;
    opt.key_file = DFL_KEY_FILE;
    opt.psk = DFL_PSK;
    opt.psk_identity = DFL_PSK_IDENTITY;
    opt.ecjpake_pw = DFL_ECJPAKE_PW;
    opt.ec_max_ops = DFL_EC_MAX_OPS;
    opt.force_ciphersuite[0] = DFL_FORCE_CIPHER;
    opt.renegotiation = DFL_RENEGOTIATION;
    opt.allow_legacy = DFL_ALLOW_LEGACY;
    opt.renegotiate = DFL_RENEGOTIATE;
    opt.exchanges = DFL_EXCHANGES;
    opt.min_version = DFL_MIN_VERSION;
    opt.max_version = DFL_MAX_VERSION;
    opt.arc4 = DFL_ARC4;
    opt.allow_sha1 = DFL_SHA1;
    opt.auth_mode = MBEDTLS_SSL_VERIFY_NONE;
    opt.mfl_code = DFL_MFL_CODE;
    opt.trunc_hmac = DFL_TRUNC_HMAC;
    opt.recsplit = DFL_RECSPLIT;
    opt.dhmlen = DFL_DHMLEN;
    opt.reconnect = DFL_RECONNECT;
    opt.reco_delay = DFL_RECO_DELAY;
    opt.reconnect_hard = DFL_RECONNECT_HARD;
    opt.tickets = DFL_TICKETS;
    opt.alpn_string = DFL_ALPN_STRING;
    opt.curves = DFL_CURVES;
    opt.transport = DFL_TRANSPORT;
    opt.hs_to_min = DFL_HS_TO_MIN;
    opt.hs_to_max = DFL_HS_TO_MAX;
    opt.dtls_mtu = DFL_DTLS_MTU;
    opt.fallback = DFL_FALLBACK;
    opt.extended_ms = DFL_EXTENDED_MS;
    opt.etm = DFL_ETM;
    opt.dgram_packing = DFL_DGRAM_PACKING;
    opt.skip_close_notify = DFL_SKIP_CLOSE_NOTIFY;

#if defined(MBEDTLS_DEBUG_C)
    mbedtls_debug_set_threshold(opt.debug_level);
#endif
    /*
     * 0. Initialize the RNG and the session data
     */
    DEBUG2( "  . Seeding the random number generator...");
    fflush(stdout);

    mbedtls_entropy_init(&entropy);
    if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func,
                                     &entropy, (const unsigned char *) pers,
                                     strlen(pers))) != 0) {
        ERROR(" failed\n  ! mbedtls_ctr_drbg_seed returned -0x%x\n", -ret);
        return 1;
    }
//    DEBUG2( " ok");
    /*
     * 1.1. Load the trusted CA
     */
    DEBUG2( "  . Loading the CA root certificate ...");

    if (strcmp(opt.ca_path, "none") == 0 ||
        strcmp(opt.ca_file, "none") == 0) {
        ret = 0;
    } else if (strlen(opt.ca_path))
        ret = mbedtls_x509_crt_parse_path(&cacert, opt.ca_path);
    else if (strlen(opt.ca_file))
        ret = mbedtls_x509_crt_parse_file(&cacert, opt.ca_file);
    else {
        for (i = 0; mbedtls_test_cas[i] != NULL; i++) {
            ret = mbedtls_x509_crt_parse(&cacert,
                                         (const unsigned char *) mbedtls_test_cas[i],
                                         mbedtls_test_cas_len[i]);
            if (ret != 0)
                break;
        }
        if (ret == 0)
            for (i = 0; mbedtls_test_cas_der[i] != NULL; i++) {
                ret = mbedtls_x509_crt_parse_der(&cacert,
                                                 (const unsigned char *) mbedtls_test_cas_der[i],
                                                 mbedtls_test_cas_der_len[i]);
                if (ret != 0)
                    break;
            }
    }
    if (ret < 0) {
        ERROR( " failed\n  !  mbedtls_x509_crt_parse returned -0x%x", -ret);
        return 1;
    }

    DEBUG2( " ok (%d skipped)", ret);

    /*
     * 1.2. Load own certificate and private key
     *
     * (can be skipped if client authentication is not required)
     */
    DEBUG2( "  . Loading the client cert. and key...");

    if (strcmp(opt.crt_file, "none") == 0)
        ret = 0;
    else if (strlen(opt.crt_file))
        ret = mbedtls_x509_crt_parse_file(&clicert, opt.crt_file);
    else
        ret = mbedtls_x509_crt_parse(&clicert,
                                     (const unsigned char *) mbedtls_test_cli_crt,
                                     mbedtls_test_cli_crt_len);
    if (ret != 0) {
        ERROR( " failed\n  !  mbedtls_x509_crt_parse returned -0x%x\n\n", -ret);
        return 1;
    }

    if (strcmp(opt.key_file, "none") == 0)
        ret = 0;
    else if (strlen(opt.key_file))
        ret = mbedtls_pk_parse_keyfile(&pkey, opt.key_file, "");
    else
        ret = mbedtls_pk_parse_key(&pkey,
                                   (const unsigned char *) mbedtls_test_cli_key,
                                   mbedtls_test_cli_key_len, NULL, 0);
    if (ret != 0) {
        ERROR( " failed\n  !  mbedtls_pk_parse_key returned -0x%x\n\n", -ret);
        return 1;
    }

    INFO( "ssl init ok");

    return 0;
}
int connect_ssl(std::string& ip, uint16_t port, int& sock) {
    int ret;
    unsigned char buf[4096];
    /*
     * 2. Start the connection
     */
    opt.server_name = "localhost";
    INFO( "  . Connecting to %s/%s/%s...",
                   opt.transport == MBEDTLS_SSL_TRANSPORT_STREAM ? "tcp" : "udp",
                   ip.c_str(), std::to_string(port).c_str());

    if ((ret = mbedtls_net_connect(&server_fd,
                                   ip.c_str(), std::to_string(port).c_str(),
                                   opt.transport == MBEDTLS_SSL_TRANSPORT_STREAM ?
                                   MBEDTLS_NET_PROTO_TCP : MBEDTLS_NET_PROTO_UDP)) != 0) {
        ERROR( " failed\n  ! mbedtls_net_connect returned -0x%x",
                       -ret);
        return 1;
    }
    sock = server_fd.fd;

    if (opt.nbio > 0)
        ret = mbedtls_net_set_nonblock(&server_fd);
    else
        ret = mbedtls_net_set_block(&server_fd);
    if (ret != 0) {
        ERROR(" failed\n  ! net_set_(non)block() returned -0x%x", -ret);
        return 1;
    }

    INFO( " ok\n");
    os_log(OS_LOG_DEFAULT, "tvpn ssl connect:%d", server_fd.fd);

    /*
     * 3. Setup stuff
     */
    INFO( "  . Setting up the SSL/TLS structure...");

    if ((ret = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, opt.transport, MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        ERROR( " failed\n  ! mbedtls_ssl_config_defaults returned -0x%x", -ret);
        return 1;
    }

    /* The default algorithms profile disables SHA-1, but our tests still
       rely on it heavily. */
    if (opt.allow_sha1 > 0) {
        crt_profile_for_test.allowed_mds |= MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA1);
        mbedtls_ssl_conf_cert_profile(&conf, &crt_profile_for_test);
        mbedtls_ssl_conf_sig_hashes(&conf, ssl_sig_hashes_for_test);
    }
    if (opt.debug_level > 0)
        mbedtls_ssl_conf_verify(&conf, my_verify, NULL);
    if (opt.auth_mode != DFL_AUTH_MODE)
        mbedtls_ssl_conf_authmode(&conf, opt.auth_mode);
    if ((ret = mbedtls_ssl_conf_max_frag_len(&conf, opt.mfl_code)) != 0) {
        ERROR(" failed! mbedtls_ssl_conf_max_frag_len returned %d", ret);
        return 1;
    }
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
    mbedtls_ssl_conf_dbg(&conf, my_debug, stdout);
    mbedtls_ssl_conf_read_timeout(&conf, opt.read_timeout);
    if (opt.force_ciphersuite[0] != DFL_FORCE_CIPHER)
        mbedtls_ssl_conf_ciphersuites(&conf, opt.force_ciphersuite);
    if (opt.allow_legacy != DFL_ALLOW_LEGACY)
        mbedtls_ssl_conf_legacy_renegotiation(&conf, opt.allow_legacy);
    if (strcmp(opt.ca_path, "none") != 0 && strcmp(opt.ca_file, "none") != 0) {
        mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
    }
    if (strcmp(opt.crt_file, "none") != 0 && strcmp(opt.key_file, "none") != 0) {
        if ((ret = mbedtls_ssl_conf_own_cert(&conf, &clicert, &pkey)) != 0) {
            ERROR( " failed\n  ! mbedtls_ssl_conf_own_cert returned %d", ret);
            return 1;
        }
    }
    if (opt.min_version != DFL_MIN_VERSION)
        mbedtls_ssl_conf_min_version(&conf, MBEDTLS_SSL_MAJOR_VERSION_3, opt.min_version);

    if (opt.max_version != DFL_MAX_VERSION)
        mbedtls_ssl_conf_max_version(&conf, MBEDTLS_SSL_MAJOR_VERSION_3, opt.max_version);

    if ((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0) {
        ERROR(" failed\n  ! mbedtls_ssl_setup returned -0x%x", -ret);
        return 1;
    }

    if ((ret = mbedtls_ssl_set_hostname(&ssl, opt.server_name)) != 0) {
        ERROR( " failed\n  ! mbedtls_ssl_set_hostname returned %d", ret);
        return 1;
    }
    mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);
   // mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, NULL, mbedtls_net_recv_timeout);
    mbedtls_ssl_set_timer_cb(&ssl, &timer, mbedtls_timing_set_delay, mbedtls_timing_get_delay);
    /*
     * 4. Handshake
     */
    INFO( "  . Performing the SSL/TLS handshake...");

    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ &&
            ret != MBEDTLS_ERR_SSL_WANT_WRITE &&
            ret != MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS) {
            ERROR( " failed! mbedtls_ssl_handshake returned -0x%x", -ret);
            if (ret == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED)
                ERROR(
                        "    Unable to verify the server's certificate. "
                        "Either it is invalid,"
                        "    or you didn't set ca_file or ca_path "
                        "to an appropriate value."
                        "    Alternatively, you may want to use "
                        "auth_mode=optional for testing purposes.");
            return 1;
        }
    }
    INFO( " ok. [ Protocol is %s ][ Ciphersuite is %s ]",
                   mbedtls_ssl_get_version(&ssl),
                   mbedtls_ssl_get_ciphersuite(&ssl));

    if ((ret = mbedtls_ssl_get_record_expansion(&ssl)) >= 0)
        INFO( "    [ Record expansion is %d ]", ret);
    else
        ERROR( "    [ Record expansion is unknown (compression) ]");

    INFO( "    [ Maximum fragment length is %u ]",
                   (unsigned int) mbedtls_ssl_get_max_frag_len(&ssl));
    /*
     * 5. Verify the server certificate
     */
    INFO( "  . Verifying peer X.509 certificate...");

    if ((flags = mbedtls_ssl_get_verify_result(&ssl)) != 0) {
        char vrfy_buf[512];
//        ERROR( " failed\n");
        mbedtls_x509_crt_verify_info(vrfy_buf, sizeof(vrfy_buf), "  ! ", flags);
        ERROR( "failed: %s", vrfy_buf);
    }

    if (mbedtls_ssl_get_peer_cert(&ssl) != NULL) {
        mbedtls_x509_crt_info((char *) buf, sizeof(buf) - 1, "      ",
                              mbedtls_ssl_get_peer_cert(&ssl));
        INFO( "  . Peer certificate information:%s", buf);
    }
    return 0;
}
int get_private_ip(int premium, std::string& androidId, std::string& userName, std::string& userPassword, std::string& recv_data) {
    /*
     * 6. Write the request
     */
    // int retry_left = opt.max_resend;
    int len, written, frags, ret;
    unsigned char *buf;

    std::string str_request;
    str_request += (char) premium;
    str_request += (char)androidId.size();
    str_request += androidId;
    if (premium >= 2) {
        str_request += (char) userName.size();
        str_request += userName;
        str_request += (char) userPassword.size();
        str_request += userPassword;
    }
    buf = (uint8_t *) (str_request.c_str());
    len = (int) (str_request.size());
    INFO("send request:%s", string_utl::HexEncode(std::string((char*)buf,len)).c_str());
    if (opt.transport == MBEDTLS_SSL_TRANSPORT_STREAM)
    {
        written = 0;
        frags = 0;

        do {
            while ((ret = mbedtls_ssl_write(&ssl, buf + written, len - written)) < 0) {
                if (ret != MBEDTLS_ERR_SSL_WANT_READ &&
                    ret != MBEDTLS_ERR_SSL_WANT_WRITE &&
                    ret != MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS) {
                    ERROR( " failed! mbedtls_ssl_write returned -0x%x", -ret);
                    return 1;
                }
            }
            frags++;
            written += ret;
        } while (written < len);
    }
    DEBUG2( " %d bytes written in %d fragments", written, frags);

    /*
     * TLS and DTLS need different reading styles (stream vs datagram)
     */
    uint8_t recv_buf[1024];
    do {
        len = sizeof(recv_buf);
        memset(recv_buf, 0, sizeof(recv_buf));
        ret = mbedtls_ssl_read(&ssl, recv_buf, len);

        if (ret == MBEDTLS_ERR_SSL_WANT_READ ||
            ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            /* For event-driven IO, wait for socket to become available */
            continue;
        }

        if (ret <= 0) {
            switch (ret) {
                case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
                    ERROR( " connection was closed gracefully");
                    return 1;
                case 0:
                case MBEDTLS_ERR_NET_CONN_RESET:
                    ERROR( " connection was reset by peer");
                    return 1;
                default:
                    ERROR( " mbedtls_ssl_read returned -0x%x", -ret);
                    return 1;
            }
        }

        len = ret;
        //DEBUG2( " %d bytes read", len);
        INFO(" %d bytes read:%s", len, string_utl::HexEncode(std::string((char*)recv_buf,len)).c_str());
        /* End of message should be detected according to the syntax of the
         * application protocol (eg HTTP), just use a dummy test here. */
        if (ret >= 4) {
            recv_data = std::string((const char *) recv_buf, ret);
            ret = 0;
            break;
        }
    } while (1);
    //mbedtls_ssl_conf_read_timeout(&conf, 500);
    return 0;
}
int ssl_close()
{
    /*
     * Cleanup and exit
     */
exit:
    mbedtls_net_free( &server_fd );

    mbedtls_x509_crt_free( &clicert );
    mbedtls_x509_crt_free( &cacert );
    mbedtls_pk_free( &pkey );
    mbedtls_ssl_session_free( &saved_session );
    mbedtls_ssl_free( &ssl );
    mbedtls_ssl_config_free( &conf );
    mbedtls_ctr_drbg_free( &ctr_drbg );
    mbedtls_entropy_free( &entropy );

    return( 0 );
}

int ssl_read(char* ip_packet_data, int& ip_packet_len)
{
    //uint8_t recv_buf[1024*16];
    const int BUF_SIZE = 4096*4;
    int ret;

    //os_log(OS_LOG_DEFAULT, "tvpn before mbedtls_ssl_read");
    ret = mbedtls_ssl_read( &ssl, (uint8_t *)ip_packet_data, BUF_SIZE);
    //os_log(OS_LOG_DEFAULT, "tvpn after mbedtls_ssl_read:%d", ret);
    if( ret <= 0 )
    {
        switch( ret )
        {
            case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
                ERROR( " connection was closed gracefully" );
                ret = 0;
                return 1;
            case 0:
            case MBEDTLS_ERR_NET_CONN_RESET:
                ERROR( " connection was reset by peer" );
                ret = 0;
                return 1;
            case MBEDTLS_ERR_SSL_TIMEOUT:
                return 0;
            default:
                ERROR( " mbedtls_ssl_read returned -0x%x", -ret );
                return 1;
        }
    }
    //DEBUG2( " %d bytes read", ret);
    /* End of message should be detected according to the syntax of the
     * application protocol (eg HTTP), just use a dummy test here. */
    if( ret > 0  )
    {
        //recv_data = std::string((const char*)recv_buf, ret);
        //write_tun((char*)recv_buf, ret);
        ip_packet_len = ret;
        return 0;
    }

    return 1;
}
int ssl_write(uint8_t* buf, int len)
{
    int written = 0;
    int frags = 0;
    int ret;
    do
    {
        while( ( ret = mbedtls_ssl_write( &ssl, buf + written, len - written ) ) < 0 )
        {
            if( ret != MBEDTLS_ERR_SSL_WANT_READ &&
                ret != MBEDTLS_ERR_SSL_WANT_WRITE &&
                ret != MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS )
            {
                ERROR( " failed! mbedtls_ssl_write returned -0x%x,%d,%d,%d", -ret ,len,written,frags);
                os_log(OS_LOG_DEFAULT,"tvpn failed! mbedtls_ssl_write returned -0x%x,%d,%d,%d", -ret ,len,written,frags);
                return 1;
            }
        }
        frags++;
        written += ret;
    } while( written < len );

    DEBUG2( " %d bytes written in %d fragments", written, frags);
    //os_log(OS_LOG_DEFAULT,"tvpn %d bytes written in %d fragments", written, frags);
    return 0;
}

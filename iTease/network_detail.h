#pragma once
#include <curl/curl.h>

#define ITEASE_DEFINE_CURLINFO(info, infotype) template<> struct CURLInfo<info> { using type = infotype; }

namespace iTease {
	namespace detail {
		template<CURLINFO info>
		struct CURLInfo;
		template<CURLINFO info>
		using CURLInfo_t = typename CURLInfo<info>::type;

		// Last URL used
		ITEASE_DEFINE_CURLINFO(CURLINFO_EFFECTIVE_URL, char *);
		// Response code
		ITEASE_DEFINE_CURLINFO(CURLINFO_RESPONSE_CODE, long);
		// Last proxy connect response code
		ITEASE_DEFINE_CURLINFO(CURLINFO_HTTP_CONNECTCODE, long);
		// Remote time of the retrieved document.
		ITEASE_DEFINE_CURLINFO(CURLINFO_FILETIME, long);
		// Total time of previous transfer.
		ITEASE_DEFINE_CURLINFO(CURLINFO_TOTAL_TIME, double);
		// Time from start until name resolving completed.
		ITEASE_DEFINE_CURLINFO(CURLINFO_NAMELOOKUP_TIME, double);
		// Time from start until remote hosst proxy completed.
		ITEASE_DEFINE_CURLINFO(CURLINFO_CONNECT_TIME, double);
		// Time fron start until SSL/SSH handshake completed.
		ITEASE_DEFINE_CURLINFO(CURLINFO_APPCONNECT_TIME, double);
		// Time from start until just before the transfter begins.
		ITEASE_DEFINE_CURLINFO(CURLINFO_PRETRANSFER_TIME, double);
		// Time from start until just when the first byte is received.
		ITEASE_DEFINE_CURLINFO(CURLINFO_STARTTRANSFER_TIME, double);
		// Time taken for all redirect steps before the final transfer.
		ITEASE_DEFINE_CURLINFO(CURLINFO_REDIRECT_TIME, double);
		// Total number of redirects that were followed.
		ITEASE_DEFINE_CURLINFO(CURLINFO_REDIRECT_COUNT, long);
		// URL a redirect would take you to, had you enabled redirects.
		ITEASE_DEFINE_CURLINFO(CURLINFO_REDIRECT_URL, char *);
		// Number of bytes uploaded (deprecated).
		ITEASE_DEFINE_CURLINFO(CURLINFO_SIZE_UPLOAD, double);
		// Number of bytes downloaded.
		ITEASE_DEFINE_CURLINFO(CURLINFO_SIZE_UPLOAD_T, curl_off_t);
		// Number of bytes downloaded (deprecated).
		ITEASE_DEFINE_CURLINFO(CURLINFO_SIZE_DOWNLOAD, double);
		// Number of bytes downloaded.
		ITEASE_DEFINE_CURLINFO(CURLINFO_SIZE_DOWNLOAD_T, curl_off_t);
		// Average download speed (deprecated).
		ITEASE_DEFINE_CURLINFO(CURLINFO_SPEED_DOWNLOAD, double);
		// Average download speed.
		ITEASE_DEFINE_CURLINFO(CURLINFO_SPEED_DOWNLOAD_T, curl_off_t);
		// Average upload speed (deprecated).
		ITEASE_DEFINE_CURLINFO(CURLINFO_SPEED_UPLOAD, double);
		// Average upload speed.
		ITEASE_DEFINE_CURLINFO(CURLINFO_SPEED_UPLOAD_T, curl_off_t);
		// Number of bytes of all headers received.
		ITEASE_DEFINE_CURLINFO(CURLINFO_HEADER_SIZE, long);
		// Number of bytes sent in the issued HTTP requests.
		ITEASE_DEFINE_CURLINFO(CURLINFO_REQUEST_SIZE, long);
		// Certificate verification result.
		ITEASE_DEFINE_CURLINFO(CURLINFO_SSL_VERIFYRESULT, long);
		// A list of OpenSSL crypto engines.
		ITEASE_DEFINE_CURLINFO(CURLINFO_SSL_ENGINES, struct curl_slist *);
		// Content length from the Content-Length header (deprecated).
		ITEASE_DEFINE_CURLINFO(CURLINFO_CONTENT_LENGTH_DOWNLOAD, long);
		// Content length from the Content-Length header.
		ITEASE_DEFINE_CURLINFO(CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, curl_off_t);
		// Upload size (deprecated).
		ITEASE_DEFINE_CURLINFO(CURLINFO_CONTENT_LENGTH_UPLOAD, long);
		// Upload size.
		ITEASE_DEFINE_CURLINFO(CURLINFO_CONTENT_LENGTH_UPLOAD_T, curl_off_t);
		// Content type from the Content-Type header.
		ITEASE_DEFINE_CURLINFO(CURLINFO_CONTENT_TYPE, char *);
		// User's private data pointer.
		ITEASE_DEFINE_CURLINFO(CURLINFO_PRIVATE, char *);
		// Avaiable HTTP authentication methods.
		ITEASE_DEFINE_CURLINFO(CURLINFO_HTTPAUTH_AVAIL, long);
		// Avaiable HTTP proxy authentication methods.
		ITEASE_DEFINE_CURLINFO(CURLINFO_PROXYAUTH_AVAIL, long);
		// The errno from the las failure to connect.
		ITEASE_DEFINE_CURLINFO(CURLINFO_OS_ERRNO, long);
		// Number of new succesful connections used for previous trasnfer.
		ITEASE_DEFINE_CURLINFO(CURLINFO_NUM_CONNECTS, long);
		// IP Address of the last connection.
		ITEASE_DEFINE_CURLINFO(CURLINFO_PRIMARY_IP, char *);
		// Port of the last connection.
		ITEASE_DEFINE_CURLINFO(CURLINFO_PRIMARY_PORT, long);
		// Local-end IP address of last connection.
		ITEASE_DEFINE_CURLINFO(CURLINFO_LOCAL_IP, char *);
		// Local-end port of last connection.
		ITEASE_DEFINE_CURLINFO(CURLINFO_LOCAL_PORT, long);
		// List of all known cookies.
		ITEASE_DEFINE_CURLINFO(CURLINFO_COOKIELIST, struct curl_slist *);
		// Last socket used.
		ITEASE_DEFINE_CURLINFO(CURLINFO_LASTSOCKET, long);
		// This option is avaiable in libcurl 7.45 or greater.
		#if defined(LIBCURL_VERSION_NUM) && LIBCURL_VERSION_NUM >= 0x072D00
		// The session's active socket.
		ITEASE_DEFINE_CURLINFO(CURLINFO_ACTIVESOCKET, curl_socket_t *);
		#endif
		// The entry path after logging in to an FTP server.
		ITEASE_DEFINE_CURLINFO(CURLINFO_FTP_ENTRY_PATH, char *);
		// Certificate chain
		ITEASE_DEFINE_CURLINFO(CURLINFO_CERTINFO, struct curl_certinfo *);
		// This costant is avaiable with libcurl < 7.48
		#if defined(LIBCURL_VERSION_NUM) && LIBCURL_VERSION_NUM < 0x073000
		// TLS session info that can be used for further processing.
		ITEASE_DEFINE_CURLINFO(CURLINFO_TLS_SESSION, struct curl_tlssessioninfo *);
		#else
		// TLS session info that can be used for further processing.
		ITEASE_DEFINE_CURLINFO(CURLINFO_TLS_SSL_PTR, struct curl_tlssessioninfo *);
		#endif
		// Whether or not a time conditional was met.
		ITEASE_DEFINE_CURLINFO(CURLINFO_CONDITION_UNMET, long);
		// RTSP session ID.
		ITEASE_DEFINE_CURLINFO(CURLINFO_RTSP_SESSION_ID, char *);
		// RTSP CSeq that will next be used.
		ITEASE_DEFINE_CURLINFO(CURLINFO_RTSP_CLIENT_CSEQ, long);
		// RTPS CSeq that will next be expected.
		ITEASE_DEFINE_CURLINFO(CURLINFO_RTSP_SERVER_CSEQ, long);
		// RTSP CSeq last received.
		ITEASE_DEFINE_CURLINFO(CURLINFO_RTSP_CSEQ_RECV, long);
	}
}
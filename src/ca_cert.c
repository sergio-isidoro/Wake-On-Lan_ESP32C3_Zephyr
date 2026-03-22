#include <zephyr/kernel.h>
#include <zephyr/net/tls_credentials.h>

/*
 * Certificado raiz usado pelo GitHub (raw.githubusercontent.com):
 * DigiCert Global Root CA
 *
 * Para atualizar:
 *   openssl s_client -connect raw.githubusercontent.com:443 -showcerts 2>/dev/null \
 *     | openssl x509 -noout -text
 * e substituir pelo certificado raiz da cadeia.
 *
 * Validade: até 2031-11-10
 */
static const unsigned char github_root_ca[] =
"-----BEGIN CERTIFICATE-----\n"
"MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh\n"
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n"
"QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT\n"
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"
"b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG\n"
"9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB\n"
"CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97\n"
"nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt\n"
"43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P\n"
"T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4\n"
"gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2YwZDAS\n"
"BgNVHRMBAf8ECDAGAQH/AgEAMA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUA95Q\n"
"NVbRTLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I9\n"
"0VUwDQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY\n"
"/EsrhMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV\n"
"5Adg06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0t\n"
"KIJFPnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujE\n"
"V0lsMkpE/1+2V8pO04LFqAi9KFBGSmINhxkB6LMdNRsA4o/cFAVFiZKOPMlCEPMo\n"
"hGAEGBFEiPEMSpnQB56GF6GvmFaMfVpakEa5DHkRRCE=\n"
"-----END CERTIFICATE-----\n";

/*
 * Regista o certificado no arranque, antes do Wi-Fi inicializar.
 * Tag 1 corresponde ao tls_tag usado em ota.c.
 */
static int register_github_cert(void) {
    return tls_credential_add(1, TLS_CREDENTIAL_CA_CERTIFICATE,
                              github_root_ca, sizeof(github_root_ca));
}

SYS_INIT(register_github_cert, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
#pragma once
#include <NetworkClientSecure.h>
#include <lwip/netdb.h>
#include "Diagnostics.h"

// Cliente exclusivo do clima: evita Network.hostByName(), cujo dns_clear_cache()
// sem lock no Arduino-ESP32 3.3.12 pode executar callbacks SNTP nesta tarefa.
// getaddrinfo usa a API de sockets thread-safe; nao manter o lock TCPIP em HTTPS.
class WeatherSecureClient : public NetworkClientSecure
{
public:
    using NetworkClientSecure::connect;

    int connect(const char* host, uint16_t port) override
    {
        IPAddress address;
        if (!address.fromString(host))
        {
            Diagnostics::event("WEATHER_DNS_START");
            struct addrinfo hints{};
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            struct addrinfo* result = nullptr;
            const int error = lwip_getaddrinfo(host, nullptr, &hints, &result);
            if (error != 0 || !result || !result->ai_addr ||
                result->ai_addrlen < sizeof(struct sockaddr_in))
            {
                if (result) lwip_freeaddrinfo(result);
                Diagnostics::event("WEATHER_DNS_FAILED");
                return 0;
            }
            address = IPAddress(reinterpret_cast<struct sockaddr_in*>(result->ai_addr)->sin_addr.s_addr);
            lwip_freeaddrinfo(result);
            Diagnostics::event("WEATHER_DNS_READY");
        }
        // Preserva o hostname para SNI e as configuracoes TLS do cliente.
        return NetworkClientSecure::connect(address, port, host, _CA_cert, _cert, _private_key);
    }

    int connect(const char* host, uint16_t port, int32_t timeout) override
    {
        _timeout = timeout;
        return connect(host, port);
    }
};

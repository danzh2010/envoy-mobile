#pragma once

#include <thread>

#include "source/extensions/transport_sockets/tls/cert_validator/default_validator.h"

#include "absl/container/flat_hash_map.h"
#include "library/common/extensions/cert_validator/platform_bridge/c_types.h"

namespace Envoy {
namespace Extensions {
namespace TransportSockets {
namespace Tls {

class PlatformBridgeCertValidator : public DefaultCertValidator {
public:
  PlatformBridgeCertValidator(const Envoy::Ssl::CertificateValidationContextConfig* config,
                              SslStats& stats, TimeSource& time_source,
                              const envoy_cert_validator* platform_bridge_api)
      : DefaultCertValidator(config, stats, time_source), platform_bridge_api_(platform_bridge_api),
        allows_expired_cert_(config != nullptr && config->allowExpiredCertificate()) {
    ENVOY_BUG(config != nullptr && config->caCert().empty() &&
                  config->certificateRevocationList().empty(),
              "Invalid cert validation context config.");
  }

  ~PlatformBridgeCertValidator() override;

  // Only called by server TLS context.
  void addClientValidationContext(SSL_CTX* /*context*/, bool /*require_client_cert*/) override {
    PANIC("not reached");
  }
  void updateDigestForSessionId(bssl::ScopedEVP_MD_CTX& /*md*/,
                                uint8_t* /*hash_buffer[EVP_MAX_MD_SIZE]*/,
                                unsigned /*hash_length*/) override {
    PANIC("not reached");
  }
  absl::optional<uint32_t> daysUntilFirstCertExpires() const override { return absl::nullopt; }
  Envoy::Ssl::CertificateDetailsPtr getCaCertInformation() const override { return nullptr; }

  // Overridden to call into platform extension API asynchronously.
  ValidationResults
  doVerifyCertChain(STACK_OF(X509) & cert_chain, Ssl::ValidateResultCallbackPtr callback,
                    Ssl::SslExtendedSocketInfo* ssl_extended_info,
                    const Network::TransportSocketOptionsConstSharedPtr& transport_socket_options,
                    SSL_CTX& ssl_ctx,
                    const CertValidator::ExtraValidationContext& validation_context,
                    bool is_server) override;

  // As CA path will not be configured, make sure the return value won’t be SSL_VERIFY_NONE because
  // of that, so that doVerifyCertChain() will be called from the TLS stack.
  int initializeSslContexts(std::vector<SSL_CTX*> contexts,
                            bool handshaker_provides_certificates) override;

  // Return empty string
  std::string getCaFileName() const override { return ""; }

  void verifyCertChainByPlatform(
      std::vector<envoy_data> certs, Ssl::ValidateResultCallbackPtr callback,
      const Network::TransportSocketOptionsConstSharedPtr transport_socket_options,
      const std::string host_name, const bool allow_expired_cert);

private:
  // latches the platform extension API.
  const envoy_cert_validator* platform_bridge_api_;
  absl::flat_hash_map<std::thread::id, std::thread> validation_threads_;
  const bool allows_expired_cert_;
};

} // namespace Tls
} // namespace TransportSockets
} // namespace Extensions
} // namespace Envoy

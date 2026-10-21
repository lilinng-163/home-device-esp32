"""生成 ESP32 配网页的本地开发证书，不覆盖已有证书和私钥。"""

import argparse
import ipaddress
from datetime import datetime, timedelta, timezone
from pathlib import Path

from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.x509.oid import NameOID


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path,
                        default=Path(__file__).resolve().parent.parent / "components/net")
    args = parser.parse_args()

    certificate_path = args.output_dir / "servercert.pem"
    key_path = args.output_dir / "prvtkey.pem"
    if certificate_path.exists() and key_path.exists():
        print("Local TLS files already exist; keeping them.")
        return
    if certificate_path.exists() or key_path.exists():
        parser.error("only one TLS file exists; restore the matching pair or use an empty directory")

    key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "home-device-esp32.local")])
    now = datetime.now(timezone.utc)
    certificate = (
        x509.CertificateBuilder()
        .subject_name(name)
        .issuer_name(name)
        .public_key(key.public_key())
        .serial_number(x509.random_serial_number())
        .not_valid_before(now - timedelta(minutes=5))
        .not_valid_after(now + timedelta(days=3650))
        .add_extension(x509.SubjectAlternativeName([
            x509.DNSName("home-device-esp32.local"),
            x509.IPAddress(ipaddress.ip_address("192.168.4.1")),
        ]), critical=False)
        .sign(key, hashes.SHA256())
    )

    args.output_dir.mkdir(parents=True, exist_ok=True)
    with key_path.open("xb") as output:
        output.write(key.private_bytes(serialization.Encoding.PEM,
                                       serialization.PrivateFormat.PKCS8,
                                       serialization.NoEncryption()))
    key_path.chmod(0o600)
    with certificate_path.open("xb") as output:
        output.write(certificate.public_bytes(serialization.Encoding.PEM))
    print("Generated local TLS files in", args.output_dir)


if __name__ == "__main__":
    main()

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(mender-mcu-integration)

target_sources(app PRIVATE src/main.c)

generate_inc_file_for_target(app "src/AmazonRootCA1.cer" "${ZEPHYR_BINARY_DIR}/include/generated/AmazonRootCA1.cer.inc")

# TODO: Make sure this is the right certificate. I got it by:
# 1) Get the trust chain for my specific Artifact URI:
#    openssl s_client -showcerts -connect c271964d41749feb10da762816c952ee.r2.cloudflarestorage.com:443
# 2) Copy the PEM format of the depth=1 chain cert "Cloudflare Inc ECC CA-3"
# 3) Convert to DER format:
#    openssl x509 -outform der -in r2.cloudflarestorage.com1.pem -out r2.cloudflarestorage.com1.crt
generate_inc_file_for_target(app "src/r2.cloudflarestorage.com1.crt" "${ZEPHYR_BINARY_DIR}/include/generated/r2.cloudflarestorage.com1.crt.inc")

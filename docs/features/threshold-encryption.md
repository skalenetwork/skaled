# Threshold Encryption

**Cryptographic scheme:**

- https://citeseerx.ist.psu.edu/document?repid=rep1&type=pdf&doi=f613e2a76843153d19adcd7c59f2766334f799bf (page 8), described below with adaptations for our specific requirements (asymmetric pairing).

**Assumptions and definitions:**

- `P_1`: `G_1` generator
- `P_2`: `G_2` generator
- `x_i`: individual BLS secret key
- `Y_i`: individual BLS public key
- `Y`: common public key
- `G_T`: a group over the `Fq_12` field

- `G: G_2 -> {0,1}^n`, `H: G_2 x {0,1}^n -> G_1` - hash functions.

**Generating ciphertext:**

- `U = r * P_2`, where `r` is a randomly chosen number.

- `V = G(r * Y) xor m`, where `m \in {0,1}^n` is a binary message of length `n`.

- `W = r * H(U, V)`

- `C = (U, V, W)` - ciphertext to be broadcast.

**Verifying ciphertext:**

- `H = H(U, V)`

- If `e(W, P_2) = e(H, U)`, the verification passes.

**Creating a decryption share:**

- `H = H(U, V)`

- If `e(W, P_2) = e(H, U)`, the verification passes.

- `U_i = x_i * U`

- `D_i = (i, U_i)` - decryption share.

**Verification and combining shares:**

- `D_i = (i, U_i)`

- If `e(P_1, U_i) = e(U, Y_i)`, the verification passes.

- `l_i` - Lagrange coefficients.

- `m = G(sum(l_i * U_i)) xor V` - plaintext.

# Threshold encryption usage

## 1. Secret bytes encryption

To generate ciphertext for a given message `plaintextBytes`:

1. Randomly generate a secret AES-256 bit key `aesKey`.

2. Encrypt the `aesKey` using the threshold encryption public key `tePubKey`, a randomly generated 256-bit integer `r` and optional `AAD_TE` (Additional Authentication Data).

   `aesKeyEncrypted = threshold_encrypt(aesKey, tePubKey, r, AAD_TE)`

3. Encrypt `plaintextBytes` using `aesKey` and optional `AAD_AES`.

   `ciphertextBytes = aes_gcm_encrypt(plaintextBytes, aesKey, AAD_AES)`

4. Construct `inputBytes` as:

   `inputBytes = aesKeyEncrypted || ciphertextBytes`

   (where `||` denotes concatenation).

## 2. Secret bytes decryption

To decrypt the encrypted message `inputBytes`:

1. Parse `inputBytes` as `inputBytes = aesKeyEncrypted || ciphertextBytes` and collect decryption shares from other parties.

2. After collecting `2/3 * N + 1` decryption shares, combine them together.

3. Decrypt `aesKey` using threshold decryption:
   `aesKey = threshold_decrypt(aesKeyEncrypted, decryption_shares, AAD_TE)`

4. Decrypt `plaintextBytes` as:
   `plaintextBytes = aes_gcm_decrypt(ciphertextBytes, aesKey, AAD_AES)`

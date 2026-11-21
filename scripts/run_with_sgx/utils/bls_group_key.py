"""Computes the DKG group's common BLS public key locally, for harnesses (like this one)
that drive the SGX wallet directly without a deployed SkaleDKG/KeyStorage contract.

In the real system, no off-chain Python code computes this key: `sgx.py` is a pure RPC
wrapper with no EC math, and skale-admin's production DKG client
(skalenetwork/skale-admin core/dkg/schain/client.py, get_common_bls_public_key()) simply
reads the already-computed key back from the chain via
`skale.key_storage.get_common_public_key()` (skalenetwork/skale.py
skale/contracts/manager/key_storage.py -> KeyStorage.getCommonPublicKey(schainHash), a
plain contract view call). The actual summation happens on-chain, in the SkaleDKG
contract's Solidity code, during the real broadcast/alright flow.

This harness has no chain, so there is nothing to read the key back from. Instead this
replicates the same algorithm the contract performs, matching the reference C++
implementation in this repo: libconsensus/libBLS/tools/dkg_glue.cpp's
GenerateSecretKeys(), which sums, over every participant, the degree-0 coefficient
commitment (index 0) of that participant's own verification vector:

    common_public_key = sum(verification_vector[node][0] for node in participants)

Verification vectors are alt_bn128 (BN254) G2 points, matching the curve py_ecc.bn128
implements - the same curve family libBLS/mcl uses under the hood.
"""

from py_ecc.bn128 import bn128_curve as curve
from py_ecc.fields import bn128_FQ2 as FQ2


def decode_g2_point(components):
    """Decodes a single G2 point as returned by SGX's getVerificationVector RPC (a list
    of 4 decimal-string field components: [x.a, x.b, y.a, y.b]) into a py_ecc affine
    point, asserting it actually lies on the twisted curve. This fails loudly on a
    component-ordering/decoding bug instead of silently producing a bogus common key."""
    if len(components) != 4:
        raise ValueError(f"Expected 4 components per G2 point, got {len(components)}: {components}")
    x = FQ2([int(components[0]), int(components[1])])
    y = FQ2([int(components[2]), int(components[3])])
    point = (x, y)
    if not curve.is_on_curve(point, curve.b2):
        raise ValueError(
            "G2 point is not on the alt_bn128 twisted curve - component ordering/decoding "
            f"is likely wrong. Raw components: {components!r}"
        )
    return point


def encode_g2_point(point):
    """Encodes a py_ecc affine G2 point back into SGX/keys_map's 4-decimal-string form."""
    x, y = point
    return [str(int(x.coeffs[0])), str(int(x.coeffs[1])), str(int(y.coeffs[0])), str(int(y.coeffs[1]))]


def sum_g2_points(points):
    """Sums a list of decoded (py_ecc) affine G2 points."""
    total = None
    for point in points:
        total = curve.add(total, point)
    if total is None:
        raise ValueError("Sum of G2 points is the point at infinity")
    return total


def compute_common_bls_public_key(verification_vectors_by_node_id, node_ids):
    """Computes the group's common BLS public key from each signing participant's own
    verification vector (as returned by SgxClient.get_verification_vector), the same way
    the SkaleDKG contract does on-chain. Returns a 4-element list of decimal strings, in
    the same shape update_config.py expects for commonPublicKey/BLSPublicKey*."""
    degree_zero_points = []
    for node_id in node_ids:
        vector = verification_vectors_by_node_id[node_id]
        if not vector:
            raise ValueError(f"Empty verification vector for nodeId={node_id}")
        degree_zero_points.append(decode_g2_point(vector[0]))

    common_point = sum_g2_points(degree_zero_points)
    return encode_g2_point(common_point)

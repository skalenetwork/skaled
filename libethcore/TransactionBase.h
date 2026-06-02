/*
    Modifications Copyright (C) 2018-2019 SKALE Labs

    This file is part of cpp-ethereum.

    cpp-ethereum is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    cpp-ethereum is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <libdevcore/Log.h>
#include <libdevcore/RLP.h>
#include <libdevcore/SHA3.h>
#include <libdevcrypto/Common.h>
#include <libethcore/Common.h>
#include <libethcore/Counter.h>
#include <libethcore/EVMSchedule.h>

#include <SkaleCommon.h>
#ifdef BITE
#include <libconsensus/node/ConsensusTypes.h>
#endif
#include <boost/optional.hpp>

namespace dev {
namespace eth {

/// Named-boolean type to encode whether a signature be included in the serialisation process.
enum IncludeSignature {
    WithoutSignature = 0,  ///< Do not include a signature.
    WithSignature = 1,     ///< Do include a signature.
};

enum class CheckTransaction { None, Cheap, Everything };

enum TransactionType { Legacy, Type1, Type2 };

/// Encodes a transaction, ready to be exported to or freshly imported from RLP.
class TransactionBase {
public:
    /// Constructs a null transaction.
    TransactionBase() {}

    /// Constructs a transaction from a transaction skeleton & optional secret.
    TransactionBase( TransactionSkeleton const& _ts, Secret const& _s = Secret() );


#ifdef FAIR

    /// Constructs a signed message-call transaction
    TransactionBase( u256 const& _value, u256 const& _gasPrice, u256 const& _gas,
        Address const& _dest, bytes const& _data, u256 const& _nonce, u256 const& _chainId,
        Secret const& _secret )
        : m_nonce( _nonce ),
          m_value( _value ),
          m_gasPrice( _gasPrice ),
          m_gas( _gas ),
          m_data( _data ),
          m_type( MessageCall ),
          m_chainId( _chainId ),
          m_receiveAddress( _dest ) {
        sign( _secret );
    }

    /// Constructs a unsigned message-call transaction
    TransactionBase( u256 const& _value, u256 const& _gasPrice, u256 const& _gas,
        Address const& _dest, bytes const& _data, u256 const& _nonce, u256 const& _chainId )
        : m_nonce( _nonce ),
          m_value( _value ),
          m_gasPrice( _gasPrice ),
          m_gas( _gas ),
          m_data( _data ),
          m_type( MessageCall ),
          m_chainId( _chainId ),
          m_receiveAddress( _dest ) {}

    /// Constructs a signed contract-creation transaction.
    TransactionBase( u256 const& _value, u256 const& _gasPrice, u256 const& _gas,
        bytes const& _data, u256 const& _nonce, u256 const& _chainId, Secret const& _secret )
        : m_nonce( _nonce ),
          m_value( _value ),
          m_gasPrice( _gasPrice ),
          m_gas( _gas ),
          m_data( _data ),
          m_type( ContractCreation ),
          m_chainId( _chainId ) {
        sign( _secret );
    }

    /// Constructs a usigned contract-creation transaction.
    TransactionBase( u256 const& _value, u256 const& _gasPrice, u256 const& _gas,
        bytes const& _data, u256 const& _nonce, u256 const& _chainId )
        : m_nonce( _nonce ),
          m_value( _value ),
          m_gasPrice( _gasPrice ),
          m_gas( _gas ),
          m_data( _data ),
          m_type( ContractCreation ),
          m_chainId( _chainId ) {}

#endif

    /// Constructs a signed message-call transaction
    TransactionBase( u256 const& _value, u256 const& _gasPrice, u256 const& _gas,
        Address const& _dest, bytes const& _data, u256 const& _nonce, Secret const& _secret )
        : m_nonce( _nonce ),
          m_value( _value ),
          m_gasPrice( _gasPrice ),
          m_gas( _gas ),
          m_data( _data ),
          m_type( MessageCall ),
          m_receiveAddress( _dest ) {
        sign( _secret );
    }


    /// Constructs a signed contract-creation transaction.
    TransactionBase( u256 const& _value, u256 const& _gasPrice, u256 const& _gas,
        bytes const& _data, u256 const& _nonce, Secret const& _secret )
        : m_nonce( _nonce ),
          m_value( _value ),
          m_gasPrice( _gasPrice ),
          m_gas( _gas ),
          m_data( _data ),
          m_type( ContractCreation ) {
        sign( _secret );
    }


    /// Constructs an unsigned message-call transaction.
    TransactionBase( u256 const& _value, u256 const& _gasPrice, u256 const& _gas,
        Address const& _dest, bytes const& _data, u256 const& _nonce = 0 )
        : m_nonce( _nonce ),
          m_value( _value ),
          m_gasPrice( _gasPrice ),
          m_gas( _gas ),
          m_data( _data ),
          m_type( MessageCall ),
          m_receiveAddress( _dest ) {}

    /// Constructs an unsigned contract-creation transaction.
    TransactionBase( u256 const& _value, u256 const& _gasPrice, u256 const& _gas,
        bytes const& _data, u256 const& _nonce = 0 )
        : m_nonce( _nonce ),
          m_value( _value ),
          m_gasPrice( _gasPrice ),
          m_gas( _gas ),
          m_data( _data ),
          m_type( ContractCreation ) {}

    /// Constructs a transaction from the given RLP.
    explicit TransactionBase(
        bytesConstRef _rlp, CheckTransaction _checkSig, bool _allowInvalid = false,
        bool _eip1559Enabled = false, bool _invalidTransactionFormatPatchEnabled = false,
        bool _berlinForkPatchEnabled = false
#ifdef BITE
        ,
        bool _bite2PatchEnabled = false
#endif
    );

    /// Constructs a transaction from the given RLP.
    explicit TransactionBase(
        bytes const& _rlp, CheckTransaction _checkSig, bool _allowInvalid = false,
        bool _eip1559Enabled = false, bool _invalidTransactionFormatPatchEnabled = false,
        bool _berlinForkPatchEnabled = false
#ifdef BITE
        ,
        bool _bite2PatchEnabled = false
#endif
        )
        : TransactionBase( &_rlp, _checkSig, _allowInvalid, _eip1559Enabled,
              _invalidTransactionFormatPatchEnabled, _berlinForkPatchEnabled
#ifdef BITE
              ,
              _bite2PatchEnabled
#endif
          ) {
    }

    TransactionBase( TransactionBase const& ) = default;

    /// Checks equality of transactions.
    bool operator==( TransactionBase const& _c ) const {
        return m_type == _c.m_type && ( safeSender() == _c.safeSender() ) &&
               ( safeNonce() == _c.safeNonce() ) &&
               ( m_type == ContractCreation || m_receiveAddress == _c.m_receiveAddress ) &&
               m_value == _c.m_value && m_data == _c.m_data;
    }

    /// Checks inequality of transactions.
    bool operator!=( TransactionBase const& _c ) const { return !operator==( _c ); }

    /// @returns sender of the transaction from the signature (and hash).
    /// @throws TransactionIsUnsigned if signature was not initialized
    Address const& sender() const;

    /// Like sender() but will never throw. @returns a null Address if the signature is invalid.
    Address const& safeSender() const noexcept;

    /// Force the sender to a particular value. This will result in an invalid transaction RLP.
    void forceSender( Address const& _a ) { m_sender = _a; }

    /// Force the chainId to a particular value. This will result in an invalid transaction RLP.
    void forceChainId( uint64_t _chainID ) { m_chainId = _chainID; }

    /// Force type. This is used in tests
    void forceType( TransactionType _type ) { m_txType = _type; }


    /// Force Type2 fees. This is used in tests
    void forceType2Fees( const u256& _maxFeePerGas, const u256& _maxPriorityFeePerGas ) {
        m_maxFeePerGas = _maxFeePerGas;
        m_maxPriorityFeePerGas = _maxPriorityFeePerGas;
    }

    /// Force gas limit. This is used in tests
    void forceGasPrice( const u256& _gasPrice ) { m_gasPrice = _gasPrice; }

#ifdef BITE

    void setDecryptedFields( const std::shared_ptr< bytes >& _decryptedData,
        const std::shared_ptr< Address >& _decryptedTo ) {
        if ( _decryptedData && _decryptedTo ) {
            m_decryptedData = _decryptedData;
            m_decryptedTo = _decryptedTo;
        }
    }

    /// @returns the decrypted data associated with this (BITE) transaction.
    bytes const& decryptedData() const;

    /// @return the decrypted address
    Address decryptedTo() const;

    // Tx is only valid BITE if is marked as BITE and has the decrypted fields set
    bool isInvalidBiteTransaction() const;

    bool isBite() const { return m_isBITETxn; }

    void checkAndValidateBITETransaction( uint64_t _epochId ) const;

    bool isCTX() const { return m_isCTX; }

    void checkIfCTXAndSet( const dev::bytes& _data );

    void setDecryptedArgsCTX( const DecryptedCTXArgs& _decryptedCTXArgs );

    void setBITE2EncryptedArgsSize( size_t _s ) { m_ctxEncryptedArgsSize = _s; }

    void setCTXOrigin( const dev::h256& _txHash ) { m_ctxOrigin = _txHash; }

    dev::h256 getCTXOrigin() const { return m_ctxOrigin; }

#endif  // BITE

    /// @throws TransactionIsUnsigned if signature was not initialized
    /// @throws InvalidSValue if the signature has an invalid S value.
    void checkLowS() const;


    /**
     * @brief Checks if the provided chain ID matches the expected value.
     *
     * This function validates the given chainId against the chain ID associated with the
     * transaction. If the chainId does not match, it throws an exception.
     *
     * @param chainId The chain ID to be checked.
     * @throws `InvalidTransactionFormat` If the transaction does not have a chainId set.
     * This should only happen if we call 'checkChainId' for pre-EIP155 transactions.
     * @throws `InvalidSignature` If the chainId does not match the expected value.
     */
    void checkChainId( uint64_t chainId ) const;

    /// @returns true if transaction is non-null.
    explicit operator bool() const { return m_type != NullTransaction && m_type != Invalid; }

    /// @returns true if transaction is contract-creation.
    bool isCreation() const { return m_type == ContractCreation; }

    /// @returns the RLP serialisation of this transaction.
    bytes toBytes( IncludeSignature _sig = WithSignature, bool _forEip155hash = false ) const {
        RLPStream s;
        streamRLP( s, _sig, _forEip155hash );
        bytes output = s.out();
        if ( m_txType != TransactionType::Legacy )
            output.insert( output.begin(), m_txType );
        return output;
    }

    /// @returns the SHA3 hash of the RLP serialisation of this transaction.
    h256 sha3( IncludeSignature _sig = WithSignature ) const;

    /// @returns the amount of ETH to be transferred by this (message-call) transaction, in Wei.
    /// Synonym for endowment().
    u256 value() const {
        CHECK_STATE2( !isInvalid(), "Transaction is invalid. Cannot get value." );
        return m_value;
    }

    /// @returns the base fee and thus the implied exchange rate of ETH to GAS.
    u256 gasPrice() const;

#ifndef FAIR
    /// @returns the non-PoW gas
    u256 nonPowGas() const;
#endif

    /// @returns the total gas to convert, paid for from sender's account. Any unused gas gets
    /// refunded once the contract is ended.
    u256 gas() const;

    /// @returns the receiving address of the message-call transaction (undefined for
    /// contract-creation transactions).
    Address receiveAddress() const {
        CHECK_STATE2( !isInvalid(), "Transaction is invalid. Cannot get receive address." );
        return m_receiveAddress;
    }

    /// Synonym for receiveAddress().
    Address to() const {
        CHECK_STATE2( !isInvalid(), "Transaction is invalid. Cannot get to address." );
        return m_receiveAddress;
    }

    /// Synonym for safeSender().
    Address from() const {
        CHECK_STATE2( !isInvalid(), "Transaction is invalid. Cannot get from address." );
        return safeSender();
    }

    /// @returns the data associated with this (message-call) transaction. Synonym for initCode().
    bytes const& data() const { return m_data; }

    /// @returns the transaction-count of the sender.
    u256 nonce() const {
        CHECK_STATE2( !isInvalid(), "Transaction is invalid. Cannot get nonce." );
        return m_nonce;
    }

    u256 safeNonce() const {
        try {
            return m_nonce;
        } catch ( ... ) {
            return u256();
        }
    }

    /// Sets the nonce to the given value. Clears any signature.
    void setNonce( u256 const& _n ) {
        CHECK_STATE2( !isInvalid(), "Transaction is invalid. Cannot set nonce." );
        clearSignature();
        m_nonce = _n;
    }

    /// @returns true if the transaction was signed
    bool hasSignature() const { return m_vrs.is_initialized(); }

    /// @returns true if the transaction was signed with zero signature
    bool hasZeroSignature() const { return m_vrs && isZeroSignature( m_vrs->r, m_vrs->s ); }

    /// @returns true if the transaction uses EIP155 replay protection
    /// Only used for non-fair builds - as fair builds reject any pre-EIP155 transactions
    bool isReplayProtected() const {
        CHECK_STATE2( !isInvalid(), "Transaction is invalid. Cannot check replay protection." );
        return m_chainId.has_value();
    }

    uint64_t chainId() const {
        CHECK_STATE2(
            m_chainId.has_value(), "Transaction does not have chainId set. Cannot get chain ID." );
        return m_chainId.get();
    }

    /// @returns the signature of the transaction (the signature has the sender encoded in it)
    /// @throws TransactionIsUnsigned if signature was not initialized
    SignatureStruct const& signature() const;

    void sign( Secret const& _priv );  ///< Sign the transaction.

    /// @returns amount of gas required for the basic payment.
    int64_t baseGasRequired( EVMSchedule const& _es ) const {
        CHECK_STATE2( !isInvalid(), "Transaction is invalid. Cannot get base gas required." );
        int64_t gasRequired = baseGasRequired( isCreation(), &m_data, _es
#ifdef BITE
            ,
            m_isBITETxn
#endif
#ifdef BITE
            ,
            m_ctxEncryptedArgsSize
#endif
        );
        if ( _es.eip2930Mode && m_txType != TransactionType::Legacy )
            gasRequired += accessListGasRequired( m_accessList, _es );
        return gasRequired;
    }

    bool isInvalid() const { return m_type == Type::Invalid; }

    TransactionType txType() const { return m_txType; }

    std::vector< bytes > accessList() const { return m_accessList; }

    u256 maxPriorityFeePerGas() const { return m_maxPriorityFeePerGas; }

    u256 maxFeePerGas() const { return m_maxFeePerGas; }

    /// Get the fee associated for a transaction with the given data.
    static int64_t baseGasRequired(
        bool _contractCreation, bytesConstRef _data, EVMSchedule const& _es
#ifdef BITE
        ,
        bool _isBITETxn = false
#endif
#ifdef BITE
        ,
        std::optional< size_t > _bite2EncryptedArgsSize = std::nullopt
#endif
    );

    static int64_t accessListGasRequired(
        std::vector< bytes > const& _accessList, EVMSchedule const& _es );

protected:
    /// Type of transaction.
    enum Type {
        NullTransaction,   ///< Null transaction.
        ContractCreation,  ///< Transaction to create contracts - receiveAddress() is ignored.
        MessageCall,       ///< Transaction to invoke a message call - receiveAddress() is used.
        Invalid            ///< Bad RLP
    };

    static bool isZeroSignature( u256 const& _r, u256 const& _s ) { return !_r && !_s; }

#ifndef FAIR
    /*
     * this function is provided in order for aleth tests and utilities to compile.
     * In will never be called in skaled since in skaled TransactionBase objects are never
     * instantiated. Aleth tests and utilities  do instantiate TransactionBase
     *
     * The function always returns zero, which means no PoW.
     */

    virtual u256 getExternalGas() const { return 0; }
#endif

    /// Clears the signature.
    void clearSignature() { m_vrs = SignatureStruct(); }

    u256 m_nonce;  ///< The transaction-count of the sender.
    u256 m_value;  ///< The amount of ETH to be transferred by this transaction. Called 'endowment'
    ///< for contract-creation transactions.
    u256 m_gasPrice;  ///< The base fee and thus the implied exchange rate of ETH to GAS.
    u256 m_gas;  ///< The total gas to convert, paid for from sender's account. Any unused gas gets
    ///< refunded once the contract is ended.
    bytes m_data;  ///< The data associated with the transaction, or the initialiser if it's a
    ///< creation transaction.
    // use shared pointer here speed up copy of transaction objects and save memory
    std::shared_ptr< bytes > m_rawData =
        std::make_shared< bytes >();    ///< Raw data, not owned by this object.>
    std::vector< bytes > m_accessList;  ///< The access list. see more
                                        ///< https://eips.ethereum.org/EIPS/eip-2930. Not valid for
                                        ///< legacy txns
    u256 m_maxPriorityFeePerGas;  ///< The maximum priority fee per gas. Only valid for type2 txns
    u256 m_maxFeePerGas;          ///< The maximum fee per gas. Only valid for type2 txns

#ifdef BITE
    std::shared_ptr< bytes > m_decryptedData = nullptr;  ///< Transaction data that was decrypted in
                                                         ///< BITE protocol
    std::shared_ptr< Address > m_decryptedTo = nullptr;  ///< Transaction to address that was
                                                         ///< decrypted in BITE protocol

    bool m_isBITETxn = false;  ///< Is this a BITE transaction

    static const Address BITE_ADDRESS;

    dev::h256 m_ctxOrigin = dev::h256( 0 );  ///< Txn that initiated submitCTX call

    std::optional< size_t > m_ctxEncryptedArgsSize = std::nullopt;
    bool m_isCTX = false;
#endif

    TransactionType m_txType = TransactionType::Legacy;

    Counter< TransactionBase > c;

private:
    /// Serialises this transaction to an RLPStream.
    /// @throws TransactionIsUnsigned if including signature was requested but it was not
    /// initialized
    void streamRLP(
        RLPStream& _s, IncludeSignature _sig = WithSignature, bool _forEip155hash = false ) const;

    static TransactionType getTransactionType( bytesConstRef _rlp );

    /// Constructs a transaction from the given RLP and transaction type.
    void fillFromBytesByType( bytesConstRef _rlpData, CheckTransaction _checkSig,
        bool _allowInvalid, TransactionType _type, bool _invalidTransactionFormatPatchEnabled,
        bool _berlinForkPatchEnabled );
    void fillFromBytesLegacy(
        bytesConstRef _rlpData, CheckTransaction _checkSig, bool _allowInvalid );
    void fillFromBytesType1( bytesConstRef _rlpData, CheckTransaction _checkSig, bool _allowInvalid,
        bool _invalidTransactionFormatPatchEnabled, bool _berlinForkPatchEnabled );
    void fillFromBytesType2( bytesConstRef _rlpData, CheckTransaction _checkSig, bool _allowInvalid,
        bool _invalidTransactionFormatPatchEnabled, bool _berlinForkPatchEnabled );

    void streamLegacyTransaction( RLPStream& _s, IncludeSignature _sig, bool _forEip155hash ) const;
    void streamType1Transaction( RLPStream& _s, IncludeSignature _sig ) const;
    void streamType2Transaction( RLPStream& _s, IncludeSignature _sig ) const;

#ifdef BITE
    // called in TransactionBase constructor
    // sets m_isBITETxn to true if a txn 'to' field
    // maches BITE address
    void checkIfBITETxnAndSet( const Address& _to );
#endif

public:
    mutable int64_t verifiedOn = -1;  // on which block it was imported

    static uint64_t howMany() { return Counter< TransactionBase >::howMany(); }

protected:
    mutable dev::Logger m_loggerDebug{ createLogger( VerbosityDebug, "TransactionBase" ) };

    Type m_type = NullTransaction;  ///< Is this a contract-creation transaction or a message-call
    ///< transaction?
    boost::optional< uint64_t > m_chainId;  ///< EIP155 value for calculating transaction hash
    ///< https://github.com/ethereum/EIPs/issues/155
    boost::optional< SignatureStruct > m_vrs;  ///< The signature of the transaction. Encodes the
    ///< sender.
    mutable h256 m_hashWith;                      ///< Cached hash of transaction with signature.
    mutable boost::optional< Address > m_sender;  ///< Cached sender, determined from signature.
    Address m_receiveAddress;                     ///< The receiving address of the transaction.
};

/// Nice name for vector of Transaction.
using TransactionBases = std::vector< TransactionBase >;

/// Simple human-readable stream-shift operator.
inline std::ostream& operator<<( std::ostream& _out, TransactionBase const& _t ) {
    _out << _t.sha3().abridged() << "{";
    if ( _t.receiveAddress() )
        _out << _t.receiveAddress().abridged();
    else
        _out << "[CREATE]";

    _out << "/" << _t.data().size() << "$" << _t.value() << "+" << _t.gas() << "@" << _t.gasPrice();
    _out << "<-" << _t.safeSender().abridged() << " #" << _t.nonce() << "}";
    return _out;
}

extern bytesConstRef bytesRefFromTransactionRlp( const RLP& _rlp );

}  // namespace eth
}  // namespace dev

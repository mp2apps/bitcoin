// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "bitcoin-config.h"
#endif

#include "init.h"

#include "addrman.h"
#include "db.h"
#include "rpcserver.h"
#include "checkpoints.h"
#include "miner.h"
#include "net.h"
#include "txdb.h"
#include "ui_interface.h"
#include "util.h"
#include "wallet.h"
#include "walletdb.h"

#include <inttypes.h>
#include <stdint.h>

#ifndef WIN32
#include <signal.h>
#endif

#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <openssl/crypto.h>

using namespace std;
using namespace boost;

static const CRPCCommand vRPCCommands[] =
{ //  name                      actor (function)         okSafeMode threadSafe reqWallet
  //  ------------------------  -----------------------  ---------- ---------- ---------
    /* Wallet */
    { "getnewaddress",          &getnewaddress,          true,      false,      true },
    { "getaccountaddress",      &getaccountaddress,      true,      false,      true },
    { "getrawchangeaddress",    &getrawchangeaddress,    true,      false,      true },
    { "setaccount",             &setaccount,             true,      false,      true },
    { "getaccount",             &getaccount,             false,     false,      true },
    { "getaddressesbyaccount",  &getaddressesbyaccount,  true,      false,      true },
    { "sendtoaddress",          &sendtoaddress,          false,     false,      true },
    { "getreceivedbyaddress",   &getreceivedbyaddress,   false,     false,      true },
    { "getreceivedbyaccount",   &getreceivedbyaccount,   false,     false,      true },
    { "listreceivedbyaddress",  &listreceivedbyaddress,  false,     false,      true },
    { "listreceivedbyaccount",  &listreceivedbyaccount,  false,     false,      true },
    { "backupwallet",           &backupwallet,           true,      false,      true },
    { "keypoolrefill",          &keypoolrefill,          true,      false,      true },
    { "walletpassphrase",       &walletpassphrase,       true,      false,      true },
    { "walletpassphrasechange", &walletpassphrasechange, false,     false,      true },
    { "walletlock",             &walletlock,             true,      false,      true },
    { "encryptwallet",          &encryptwallet,          false,     false,      true },
    { "validateaddress",        &validateaddress,        true,      false,      false },
    { "getbalance",             &getbalance,             false,     false,      true },
    { "move",                   &movecmd,                false,     false,      true },
    { "sendfrom",               &sendfrom,               false,     false,      true },
    { "sendmany",               &sendmany,               false,     false,      true },
    { "addmultisigaddress",     &addmultisigaddress,     false,     false,      true },
    { "createmultisig",         &createmultisig,         true,      true ,      false },
    { "gettransaction",         &gettransaction,         false,     false,      true },
    { "listtransactions",       &listtransactions,       false,     false,      true },
    { "listaddressgroupings",   &listaddressgroupings,   false,     false,      true },
    { "signmessage",            &signmessage,            false,     false,      true },
    { "verifymessage",          &verifymessage,          false,     false,      false },
    { "listaccounts",           &listaccounts,           false,     false,      true },
    { "listsinceblock",         &listsinceblock,         false,     false,      true },
    { "dumpprivkey",            &dumpprivkey,            true,      false,      true },
    { "dumpwallet",             &dumpwallet,             true,      false,      true },
    { "importprivkey",          &importprivkey,          false,     false,      true },
    { "importwallet",           &importwallet,           false,     false,      true },
    { "listunspent",            &listunspent,            false,     false,      true },
    { "lockunspent",            &lockunspent,            false,     false,      true },
    { "listlockunspent",        &listlockunspent,        false,     false,      true },

    /* Wallet-enabled mining */
    { "getgenerate",            &getgenerate,            true,      false,      false },
    { "setgenerate",            &setgenerate,            true,      true,       false },
    { "gethashespersec",        &gethashespersec,        true,      false,      false },
    { "getwork",                &getwork,                true,      false,      true },
};

void WalletRegisterRPCCommands()
{
    tableRPC.RegisterMethodList(vRPCCommands, (sizeof(vRPCCommands) / sizeof(vRPCCommands[0])));
}


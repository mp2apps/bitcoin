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
using namespace json_spirit;

std::string strWalletFile;
CWallet* pwalletMain;

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

class CWalletBackend: public CWalletBackendInterface
{
public:
    bool ParseAndValidateArguments();
    bool VerifyWallets(std::ostringstream &strErrors);
    bool LoadWallets(std::ostringstream &strErrors);
    void ShowStartupStatistics();
    void GenerateBitcoins(bool fGenerate, int nThreads);
    void InitializePostNodeStart(boost::thread_group& threadGroup);
    void ShutdownPreNodeStop();
    void ShutdownPostNodeStop();
    void DeleteWallets();
    void RegisterRPCCommands();
    void GetInfo(void *obj);
    void GetMiningInfo(void *obj);
    void EnsureWalletIsUnlocked();
    const CKeyStore *GetKeystore();
};

bool static InitError(const std::string &str)
{
    uiInterface.ThreadSafeMessageBox(str, "", CClientUIInterface::MSG_ERROR);
    return false;
}

bool static InitWarning(const std::string &str)
{
    uiInterface.ThreadSafeMessageBox(str, "", CClientUIInterface::MSG_WARNING);
    return true;
}


bool CWalletBackend::ParseAndValidateArguments()
{
    std::string strDataDir = GetDataDir().string();
    strWalletFile = GetArg("-wallet", "wallet.dat");
    // Wallet file must be a plain filename without a directory
    if (strWalletFile != boost::filesystem::basename(strWalletFile) + boost::filesystem::extension(strWalletFile))
        return InitError(strprintf(_("Wallet %s resides outside data directory %s"), strWalletFile.c_str(), strDataDir.c_str()));
    return true;
}

bool CWalletBackend::VerifyWallets(std::ostringstream &strErrors)
{
    std::string strDataDir = GetDataDir().string();
    uiInterface.InitMessage(_("Verifying wallet..."));

    if (!bitdb.Open(GetDataDir()))
    {
        // try moving the database env out of the way
        boost::filesystem::path pathDatabase = GetDataDir() / "database";
        boost::filesystem::path pathDatabaseBak = GetDataDir() / strprintf("database.%"PRId64".bak", GetTime());
        try {
            boost::filesystem::rename(pathDatabase, pathDatabaseBak);
            LogPrintf("Moved old %s to %s. Retrying.\n", pathDatabase.string().c_str(), pathDatabaseBak.string().c_str());
        } catch(boost::filesystem::filesystem_error &error) {
             // failure is ok (well, not really, but it's not worse than what we started with)
        }

        // try again
        if (!bitdb.Open(GetDataDir())) {
            // if it still fails, it probably means we can't even create the database env
            string msg = strprintf(_("Error initializing wallet database environment %s!"), strDataDir.c_str());
            return InitError(msg);
        }
    }

    if (GetBoolArg("-salvagewallet", false))
    {
        // Recover readable keypairs:
        if (!CWalletDB::Recover(bitdb, strWalletFile, true))
            return false;
    }

    if (filesystem::exists(GetDataDir() / strWalletFile))
    {
        CDBEnv::VerifyResult r = bitdb.Verify(strWalletFile, CWalletDB::Recover);
        if (r == CDBEnv::RECOVER_OK)
        {
            string msg = strprintf(_("Warning: wallet.dat corrupt, data salvaged!"
                                     " Original wallet.dat saved as wallet.{timestamp}.bak in %s; if"
                                     " your balance or transactions are incorrect you should"
                                     " restore from a backup."), strDataDir.c_str());
            InitWarning(msg);
        }
        if (r == CDBEnv::RECOVER_FAIL)
            return InitError(_("wallet.dat corrupt, salvage failed"));
    }
    return true;
}

bool CWalletBackend::LoadWallets(std::ostringstream &strErrors)
{
    uiInterface.InitMessage(_("Loading wallet..."));

    int64_t nStart = GetTimeMillis();
    bool fFirstRun = true;
    pwalletMain = new CWallet(strWalletFile);
    DBErrors nLoadWalletRet = pwalletMain->LoadWallet(fFirstRun);
    if (nLoadWalletRet != DB_LOAD_OK)
    {
        if (nLoadWalletRet == DB_CORRUPT)
            strErrors << _("Error loading wallet.dat: Wallet corrupted") << "\n";
        else if (nLoadWalletRet == DB_NONCRITICAL_ERROR)
        {
            string msg(_("Warning: error reading wallet.dat! All keys read correctly, but transaction data"
                         " or address book entries might be missing or incorrect."));
            InitWarning(msg);
        }
        else if (nLoadWalletRet == DB_TOO_NEW)
            strErrors << _("Error loading wallet.dat: Wallet requires newer version of Bitcoin") << "\n";
        else if (nLoadWalletRet == DB_NEED_REWRITE)
        {
            strErrors << _("Wallet needed to be rewritten: restart Bitcoin to complete") << "\n";
            LogPrintf("%s", strErrors.str().c_str());
            return InitError(strErrors.str());
        }
        else
            strErrors << _("Error loading wallet.dat") << "\n";
    }

    if (GetBoolArg("-upgradewallet", fFirstRun))
    {
        int nMaxVersion = GetArg("-upgradewallet", 0);
        if (nMaxVersion == 0) // the -upgradewallet without argument case
        {
            LogPrintf("Performing wallet upgrade to %i\n", FEATURE_LATEST);
            nMaxVersion = CLIENT_VERSION;
            pwalletMain->SetMinVersion(FEATURE_LATEST); // permanently upgrade the wallet immediately
        }
        else
            LogPrintf("Allowing wallet upgrade up to %i\n", nMaxVersion);
        if (nMaxVersion < pwalletMain->GetVersion())
            strErrors << _("Cannot downgrade wallet") << "\n";
        pwalletMain->SetMaxVersion(nMaxVersion);
    }

    if (fFirstRun)
    {
        // Create new keyUser and set as default key
        RandAddSeedPerfmon();

        CPubKey newDefaultKey;
        if (pwalletMain->GetKeyFromPool(newDefaultKey)) {
            pwalletMain->SetDefaultKey(newDefaultKey);
            if (!pwalletMain->SetAddressBook(pwalletMain->vchDefaultKey.GetID(), "", "receive"))
                strErrors << _("Cannot write default address") << "\n";
        }

        pwalletMain->SetBestChain(chainActive.GetLocator());
    }

    LogPrintf("%s", strErrors.str().c_str());
    LogPrintf(" wallet      %15"PRId64"ms\n", GetTimeMillis() - nStart);

    RegisterWallet(pwalletMain);

    CBlockIndex *pindexRescan = chainActive.Tip();
    if (GetBoolArg("-rescan", false))
        pindexRescan = chainActive.Genesis();
    else
    {
        CWalletDB walletdb(strWalletFile);
        CBlockLocator locator;
        if (walletdb.ReadBestBlock(locator))
            pindexRescan = chainActive.FindFork(locator);
        else
            pindexRescan = chainActive.Genesis();
    }
    if (chainActive.Tip() && chainActive.Tip() != pindexRescan)
    {
        uiInterface.InitMessage(_("Rescanning..."));
        LogPrintf("Rescanning last %i blocks (from block %i)...\n", chainActive.Height() - pindexRescan->nHeight, pindexRescan->nHeight);
        nStart = GetTimeMillis();
        pwalletMain->ScanForWalletTransactions(pindexRescan, true);
        LogPrintf(" rescan      %15"PRId64"ms\n", GetTimeMillis() - nStart);
        pwalletMain->SetBestChain(chainActive.GetLocator());
        nWalletDBUpdated++;
    }
    return true;
}

void CWalletBackend::ShowStartupStatistics()
{
    LogPrintf("setKeyPool.size() = %"PRIszu"\n",      pwalletMain ? pwalletMain->setKeyPool.size() : 0);
    LogPrintf("mapWallet.size() = %"PRIszu"\n",       pwalletMain ? pwalletMain->mapWallet.size() : 0);
    LogPrintf("mapAddressBook.size() = %"PRIszu"\n",  pwalletMain ? pwalletMain->mapAddressBook.size() : 0);
}

void CWalletBackend::GenerateBitcoins(bool fGenerate, int nThreads)
{
    if(pwalletMain)
        ::GenerateBitcoins(fGenerate, pwalletMain, nThreads);
}

void CWalletBackend::InitializePostNodeStart(boost::thread_group& threadGroup)
{
    // Add wallet transactions that aren't already in a block to mapTransactions
    pwalletMain->ReacceptWalletTransactions();

    // Run a thread to flush wallet periodically
    threadGroup.create_thread(boost::bind(&ThreadFlushWalletDB, boost::ref(pwalletMain->strWalletFile)));
}

void CWalletBackend::ShutdownPreNodeStop()
{
    if (pwalletMain)
        bitdb.Flush(false);
    ::GenerateBitcoins(false, NULL, 0);
}

void CWalletBackend::ShutdownPostNodeStop()
{
    if (pwalletMain)
        bitdb.Flush(true);
}

void CWalletBackend::DeleteWallets()
{
    if (pwalletMain)
        delete pwalletMain;
}

void CWalletBackend::RegisterRPCCommands()
{
    tableRPC.RegisterMethodList(vRPCCommands, (sizeof(vRPCCommands) / sizeof(vRPCCommands[0])));

    // InitRPCMining is needed here so getwork/getblocktemplate in the GUI debug console works properly.
    InitRPCMining();
}

void CWalletBackend::GetInfo(void *objp)
{
    Object &obj = *(Object*)objp;
    if (pwalletMain) {
        obj.push_back(Pair("walletversion", pwalletMain->GetVersion()));
        obj.push_back(Pair("balance",       ValueFromAmount(pwalletMain->GetBalance())));
        obj.push_back(Pair("keypoololdest", (boost::int64_t)pwalletMain->GetOldestKeyPoolTime()));
        obj.push_back(Pair("keypoolsize",   (int)pwalletMain->GetKeyPoolSize()));
        if (pwalletMain->IsCrypted())
            obj.push_back(Pair("unlocked_until", (boost::int64_t)nWalletUnlockTime));
    }
}

void CWalletBackend::GetMiningInfo(void *objp)
{
    Object &obj = *(Object*)objp;
    Array params;
    obj.push_back(Pair("generate",         getgenerate(params, false)));
    obj.push_back(Pair("hashespersec",     gethashespersec(params, false)));
}

void CWalletBackend::EnsureWalletIsUnlocked()
{
    ::EnsureWalletIsUnlocked();
}

const CKeyStore *CWalletBackend::GetKeystore()
{
    return pwalletMain;
}

CWalletBackend walletBackend;

/* Get functional wallet backend */
CWalletBackendInterface *GetWalletBackend()
{
    return &walletBackend;
}


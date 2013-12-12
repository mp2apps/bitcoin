// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_INIT_H
#define BITCOIN_INIT_H

#include <string>

class CWallet;
class CKeyStore;

namespace boost {
    class thread_group;
};

void StartShutdown();
bool ShutdownRequested();
void Shutdown();
bool AppInit2(boost::thread_group& threadGroup, bool fForceServer);

/* The help message mode determines what help message to show */
enum HelpMessageMode
{
    HMM_BITCOIND,
    HMM_BITCOIN_QT
};

std::string HelpMessage(HelpMessageMode mode);

/** Interface object for wallet backend.
 * The wallet backend is notified at different places in the
 * initialization and shutdown process, and can provide data for some
 * generic RPC calls.
 * It can also register new RPC calls.
 */
class CWalletBackendInterface
{
public:
    //// Init/Shutdown ////
    /// Parse and store command line arguments
    virtual bool ParseAndValidateArguments() = 0;
    /// Verify wallet(s)
    virtual bool VerifyWallets(std::ostringstream &strErrors) = 0;
    /// Load wallet file(s), create a wallet interface and register it
    virtual bool LoadWallets(std::ostringstream &strErrors) = 0;
    /// Show startup statistics for wallet(s)
    virtual void ShowStartupStatistics() = 0;
    /// Start generating coins in the background
    virtual void GenerateBitcoins(bool fGenerate, int nThreads) = 0;
    /// Prepare wallet(s) post node start
    virtual void InitializePostNodeStart(boost::thread_group& threadGroup) = 0;
    /// Shutdown sequence before node stop
    virtual void ShutdownPreNodeStop() = 0;
    /// Shutdown sequence after node stop
    virtual void ShutdownPostNodeStop() = 0;
    /// Delete the wallet object(s)
    virtual void DeleteWallets() = 0;

    //// RPC ////
    /// Register RPC commands
    virtual void RegisterRPCCommands() = 0;
    /// Add information to getinfo call
    virtual void GetInfo(void *obj) = 0;
    /// Add information to getmininginfo call
    virtual void GetMiningInfo(void *obj) = 0;
    /// Raise an error if the wallet is locked
    virtual void EnsureWalletIsUnlocked() = 0;
    /// Get keystore for the main wallet, used by signrawtransaction
    virtual const CKeyStore *GetKeystore() = 0;
};

extern CWalletBackendInterface *pwalletBackend;

#endif

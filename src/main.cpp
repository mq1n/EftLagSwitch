#include "random.hpp"
#include "FirewallHellper.hpp"

#define USE_LOOP_TIMER      true
#define BASE_TIMEOUT_MSEC   3500

#define FW_RULE_NAME        L"EFT-LagSwitch"
#define FW_RULE_DESC        L"EFT-LagSwitch"
#define FW_RULE_GROUP_NAME  L"EFT"
#define FW_RULE_TARGET_PROC L"D:\\Battlestate Games\\EFT (live)\\EscapeFromTarkov.exe"

#define ENABLE_KEY_CODE     VK_XBUTTON1 /* Right macro key of mouse */
#define DISABLE_KEY_CODE    VK_XBUTTON2 /* Left macro key of mouse */
#define BREAK_KEY_CODE      VK_MBUTTON  /* Middle mouse key */

static HANDLE gs_hTimer = nullptr;

struct TimerContext
{
    CFirewallHelper* fwMgr;
    INetFwRule* fwRule;
    INetFwRule** fwRuleLocalPtr;
};

void NTAPI WatchdogTimer(PVOID lpParameter, BOOLEAN TimerOrWaitFired)
{
    std::cout << "Rule watchdog timeout!!!" << std::endl;

    if (lpParameter)
    {
        auto ctx = reinterpret_cast<TimerContext*>(lpParameter);

#if USE_LOOP_TIMER == true
        const auto hr = ctx->fwMgr->EnableRule(ctx->fwRule, ctx->fwMgr->IsEnabledRule(ctx->fwRule) ? false : true);
        if (FAILED(hr) || hr == S_FALSE)
            std::cout << "Rule status can not changed!" << std::endl;
        else
            std::cout << "Rule status has been changed!" << std::endl;
#else
        auto hr = ctx->fwMgr->EnableRule(ctx->fwRule, false);
        if (FAILED(hr) || hr == S_FALSE)
        {
            std::cout << "Rule can not disabled!" << std::endl;
        }
        else
        {
            std::cout << "Rule succesfully disabled!" << std::endl;

            if (FAILED(ctx->fwMgr->RemoveRule(FW_RULE_NAME)))
                std::cout << "Rule can not removed!" << std::endl;
            else
                std::cout << "Rule succesfully removed!" << std::endl;

            if (ctx->fwRule)
            {
                ctx->fwRule->Release();
                ctx->fwRule = nullptr;
            }
            *ctx->fwRuleLocalPtr = nullptr;
        }

        DeleteTimerQueueTimer(nullptr, gs_hTimer, nullptr);
        gs_hTimer = nullptr;
#endif
    }
}

int main()
{
    std::cout << "STARTED!" << std::endl;

    using Random = effolkronium::random_static;
    CFirewallHelper fwMgr{};

    auto fwOn = false;
    if (FAILED(fwMgr.IsFirewallEnabled(fwOn)) || !fwOn)
    {
        std::cout << "Windows firewall must be on!" << std::endl;

        if (FAILED(fwMgr.ManageFirewallState(true)))
        {
            std::cout << "Windows firewall could not be turn on!" << std::endl;
            abort();
        }
    }

    fwMgr.EnumerateRules(
        [](INetFwRule* pFwRule, void* pvUserContext) -> bool {
            BSTR bstrVal{};
            if (SUCCEEDED(pFwRule->get_Name(&bstrVal)))
            {
                if (!wcscmp(FW_RULE_NAME, bstrVal))
                {
                    std::cout << "The Firewall rule is already exist, Old rule has been removed!" << std::endl;
                    pFwRule->Release();
                }
            }
            return true;
        }, nullptr
    );

    auto hr = HRESULT{ S_OK };
    INetFwRule* rulePtr = nullptr;
    do
    {
        if (GetAsyncKeyState(ENABLE_KEY_CODE) & 0x8000)
        {
            while (GetAsyncKeyState(ENABLE_KEY_CODE) & 0x8000)
                Sleep(1);

            fwMgr.EnumerateRules(
                [&rulePtr](INetFwRule* pFwRule, void* pvUserContext) -> bool {
                    BSTR bstrVal{};
                    if (SUCCEEDED(pFwRule->get_Name(&bstrVal)))
                    {
                        if (!wcscmp(FW_RULE_NAME, bstrVal))
                        {
                            rulePtr = pFwRule;
                            return false;
                        }
                    }
                    return true;
                }, nullptr
            );

            auto bWatch = false;
            if (!rulePtr)
            {
                if (FAILED(fwMgr.AddRule(FW_RULE_NAME, FW_RULE_DESC, FW_RULE_GROUP_NAME, FW_RULE_TARGET_PROC, &rulePtr)))
                {
                    std::cout << "The firewall rule could not be created!" << std::endl;
                    abort();
                }
                else
                {
                    std::cout << "The Firewall rule is succesfully created! Ptr: " << std::hex << rulePtr << std::endl;
                    bWatch = true;
                }
            }
            else
            {
                if (FAILED(hr = fwMgr.EnableRule(rulePtr, true)) || hr == S_FALSE)
                {
                    std::cout << "Rule can not enabled!" << std::endl;
                }
                else
                {
                    std::cout << "Rule succesfully enabled!" << std::endl;
                    bWatch = true;
                }
            }

            if (bWatch)
            {
                TimerContext ctx{ &fwMgr, rulePtr, &rulePtr };
#if USE_LOOP_TIMER == true
                if (!CreateTimerQueueTimer(&gs_hTimer, nullptr, WatchdogTimer, &ctx, 0, BASE_TIMEOUT_MSEC + Random::get(10, 600), WT_EXECUTEDEFAULT))
#else
                if (!CreateTimerQueueTimer(&gs_hTimer, nullptr, WatchdogTimer, &ctx, BASE_TIMEOUT_MSEC + Random::get(10, 600), 0, WT_EXECUTEDEFAULT))
#endif
                {
                    std::cout << "Rule watchdog timer can not enabled! Error: " << GetLastError() << std::endl;
                }
                else
                {
                    std::cout << "Rule watchdog timer created! Handle: " << std::hex << gs_hTimer << std::endl;
                }
            }
        }
        else if (GetAsyncKeyState(DISABLE_KEY_CODE) & 0x8000)
        {
            while (GetAsyncKeyState(DISABLE_KEY_CODE) & 0x8000)
                Sleep(1);

            if (rulePtr)
            {
                if (gs_hTimer)
                {
                    DeleteTimerQueueTimer(nullptr, gs_hTimer, nullptr);
                    gs_hTimer = nullptr;
                }
                if (fwMgr.IsEnabledRule(rulePtr))
                {
                    if (FAILED(hr = fwMgr.EnableRule(rulePtr, false)) || hr == S_FALSE)
                    {
                        std::cout << "Rule can not disabled!" << std::endl;
                    }
                    else
                    {
                        std::cout << "Rule succesfully disabled!" << std::endl;

                        fwMgr.RemoveRule(FW_RULE_NAME);

                        if (rulePtr)
                        {
                            rulePtr->Release();
                            rulePtr = nullptr;
                        }
                    }
                }
            }
        }
        /*
        else if (GetAsyncKeyState(BREAK_KEY_CODE) & 0x8000)
        {
            while (GetAsyncKeyState(BREAK_KEY_CODE) & 0x8000)
                Sleep(1);

            break;
        }
        */

        Sleep(100);

    } while (true);

    if (gs_hTimer)
        DeleteTimerQueueTimer(nullptr, gs_hTimer, nullptr);

    std::cout << "COMPLETED!" << std::endl;
    std::system("PAUSE");

    return EXIT_SUCCESS;
}

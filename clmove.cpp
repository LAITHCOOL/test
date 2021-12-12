typedef void(__cdecl* clMove_fn)(float, bool);

void __cdecl hooks::Hooked_CLMove(float flAccumulatedExtraSamples, bool bFinalTick)
{

  

    if(g_ctx.globals.fakeducking)
        return (clMove_fn(hooks::original_clmove)(flAccumulatedExtraSamples, bFinalTick));


    if (g_ctx.globals.startcharge && g_ctx.globals.tocharge < g_ctx.globals.tochargeamount)
    {
        g_ctx.globals.tocharge++;
        g_ctx.globals.ticks_allowed = g_ctx.globals.tocharge;
        m_globals()->m_interpolation_amount = 0.f;
        return;
    }

    (clMove_fn(hooks::original_clmove)(flAccumulatedExtraSamples, bFinalTick));


        while (g_ctx.globals.shift_ticks)
        {
            g_ctx.globals.isshifting = true;
            g_ctx.globals.shift_ticks--;
            g_ctx.globals.tocharge--;
            (clMove_fn(hooks::original_clmove)(flAccumulatedExtraSamples, bFinalTick));
        }
        g_ctx.globals.isshifting = false;

      
}

void misc::double_tap(CUserCmd* m_pcmd)
{
    static auto lastdoubletaptime = 0;
    if (!g_cfg.ragebot.double_tap_key.key) {
        g_ctx.globals.shift_ticks = g_ctx.globals.tocharge;
        return;
    }

    g_ctx.globals.tickbase_shift = 16;

    auto weapon = g_ctx.local()->m_hActiveWeapon();

    if (!(m_pcmd->m_buttons & IN_ATTACK) && g_ctx.globals.tocharge < 16 && g_ctx.globals.fixed_tickbase - lastdoubletaptime > TIME_TO_TICKS(0.75f)) {
        g_ctx.globals.startcharge = true;
        g_ctx.globals.tochargeamount = 16;
    }
    else {
        g_ctx.globals.startcharge = false;
    }

    if (g_ctx.globals.tocharge > 16)
        g_ctx.globals.shift_ticks = g_ctx.globals.tocharge - 16;

    if (weapon && (m_pcmd->m_buttons & IN_ATTACK || (m_pcmd->m_buttons & IN_ATTACK2 && weapon->is_knife())) && g_ctx.globals.tocharge == 16) {
        lastdoubletaptime = g_ctx.globals.fixed_tickbase;
        g_ctx.globals.shift_ticks = 16;
    }
}


void misc::lagexploit(CUserCmd* m_pcmd)
{
    if (fakelag::get().ispeeking() && g_cfg.ragebot.lagpeek)
    {
        if (g_ctx.globals.shift_timer < 1)
        {
            g_ctx.globals.tickbase_shift = 0;
            ++g_ctx.globals.shift_timer;
        }
        if (g_ctx.globals.shift_timer > 0)
        {
            g_ctx.globals.tickbase_shift = 16;
        }
    }
    else
    {
        g_ctx.globals.shift_timer = 0;
    }
}
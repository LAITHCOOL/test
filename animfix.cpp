if (!player_info.fakeplayer && g_ctx.local()->is_alive() && e->m_iTeamNum() != g_ctx.local()->m_iTeamNum() && !g_cfg.legitbot.enabled)
    {




        animstate->m_flGoalFeetYaw = previous_goal_feet_yaw[e->EntIndex()];

        g_ctx.globals.updating_animation = true;
        e->update_clientside_animation();
        g_ctx.globals.updating_animation = false;

        previous_goal_feet_yaw[e->EntIndex()] = animstate->m_flGoalFeetYaw;
        memcpy(animstate, &state, sizeof(c_baseplayeranimationstate));

        animstate->m_flGoalFeetYaw = math::normalize_yaw(e->m_angEyeAngles().y);
        g_ctx.globals.updating_animation = true;
        e->m_bClientSideAnimation() = true;
        e->update_clientside_animation();
        e->m_bClientSideAnimation() = false;
        g_ctx.globals.updating_animation = false;
        setup_matrix(e, animlayers, NONE);
        memcpy(animstate, &state, sizeof(c_baseplayeranimationstate));
        memcpy(player_resolver[e->EntIndex()].moveLayers[0], e->get_animlayers(), e->animlayer_count() * sizeof(AnimationLayer));

        animstate->m_flGoalFeetYaw = math::normalize_yaw(e->m_angEyeAngles().y + e->get_max_desync_delta());
        g_ctx.globals.updating_animation = true;
        e->m_bClientSideAnimation() = true;
        e->update_clientside_animation();
        e->m_bClientSideAnimation() = false;
        g_ctx.globals.updating_animation = false;
        setup_matrix(e, animlayers, FIRST);
        memcpy(animstate, &state, sizeof(c_baseplayeranimationstate));
        memcpy(player_resolver[e->EntIndex()].moveLayers[1], e->get_animlayers(), e->animlayer_count() * sizeof(AnimationLayer));

        animstate->m_flGoalFeetYaw = math::normalize_yaw(e->m_angEyeAngles().y - e->get_max_desync_delta());
        g_ctx.globals.updating_animation = true;
        e->m_bClientSideAnimation() = true;
        e->update_clientside_animation();
        e->m_bClientSideAnimation() = false;
        g_ctx.globals.updating_animation = false;
        setup_matrix(e, animlayers, SECOND);
        memcpy(animstate, &state, sizeof(c_baseplayeranimationstate));
        memcpy(player_resolver[e->EntIndex()].moveLayers[2], e->get_animlayers(), e->animlayer_count() * sizeof(AnimationLayer));


        player_resolver[e->EntIndex()].initialize(e, record, previous_goal_feet_yaw[e->EntIndex()], e->m_angEyeAngles().x);
        player_resolver[e->EntIndex()].resolve_yaw();

        if (g_cfg.player_list.low_delta[e->EntIndex()])
        {
            switch (record->side)
            {
            case RESOLVER_FIRST:
                record->side = RESOLVER_LOW_FIRST;
                break;
            case RESOLVER_SECOND:
                record->side = RESOLVER_LOW_SECOND;
                break;
            case RESOLVER_LOW_FIRST:
                record->side = RESOLVER_FIRST;
                break;
            case RESOLVER_LOW_SECOND:
                record->side = RESOLVER_SECOND;
                break;
            }
        }

        switch (record->side)
        {
        case RESOLVER_ORIGINAL:
            animstate->m_flGoalFeetYaw = previous_goal_feet_yaw[e->EntIndex()];
            break;
        case RESOLVER_ZERO:
            animstate->m_flGoalFeetYaw = math::normalize_yaw(e->m_angEyeAngles().y);
            break;
        case RESOLVER_FIRST:
            animstate->m_flGoalFeetYaw = math::normalize_yaw(e->m_angEyeAngles().y + e->get_max_desync_delta());
            break;
        case RESOLVER_SECOND:
            animstate->m_flGoalFeetYaw = math::normalize_yaw(e->m_angEyeAngles().y - e->get_max_desync_delta());
            break;
        case RESOLVER_LOW_FIRST:
            animstate->m_flGoalFeetYaw = math::normalize_yaw(e->m_angEyeAngles().y + (e->get_max_desync_delta()) * 0.5);
            break;
        case RESOLVER_LOW_SECOND:
            animstate->m_flGoalFeetYaw = math::normalize_yaw(e->m_angEyeAngles().y - (e->get_max_desync_delta()) * 0.5);
            break;
        }

        e->m_angEyeAngles().x = player_resolver[e->EntIndex()].resolve_pitch();
    }
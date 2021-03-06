#include "AnimationSync.hpp"
#include "LagCompensation.hpp"
#include "BoneManager.hpp"

void C_AnimSync::Instance( ClientFrameStage_t Stage )
{
	if ( Stage != ClientFrameStage_t::FRAME_NET_UPDATE_END )
		return;

	int32_t iPlayerIndex = 0;
	do
	{
		iPlayerIndex++;

		C_BasePlayer* pPlayer = C_BasePlayer::GetPlayerByIndex( iPlayerIndex );
		if ( !pPlayer || !pPlayer->IsPlayer( ) || pPlayer->IsDormant( ) || !pPlayer->IsAlive( ) || pPlayer == g_Globals.m_Local || pPlayer->GetCSPlayer( )->m_bGunGameImmunity( ) || ( pPlayer->m_fFlags( ) & FL_FROZEN ) )
			continue;

		std::deque < C_LagRecord >& m_LagRecords = g_LagCompensation->GetPlayerRecords( iPlayerIndex );
		if ( m_LagRecords.empty( ) )
			continue;
		
		C_LagRecord& m_LatestRecord = m_LagRecords.back( );
		if ( pPlayer->m_flSimulationTime( ) <= pPlayer->m_flOldSimulationTime( ) )
			continue;
	
		C_LagRecord m_PreviousRecord = C_LagRecord( );
		if ( g_PlayerData[ iPlayerIndex ].m_bHasPreviousRecord )
		{
			m_PreviousRecord = g_PlayerData[ iPlayerIndex ].m_PreviousRecord;
			if ( m_PreviousRecord.m_AnimationLayers.at( ANIMATION_LAYER_ALIVELOOP ).m_flCycle == m_LatestRecord.m_AnimationLayers.at( ANIMATION_LAYER_ALIVELOOP ).m_flCycle )
				continue;
		}

		// save game shit
		std::array < float_t, 24 > aOldPoseParameters = { };
		std::array < C_AnimationLayer, ANIMATION_LAYER_COUNT > aOldAnimationLayers = { };
		C_CSGOPlayerAnimationState AnimationState_NULL = C_CSGOPlayerAnimationState( );

		std::memcpy( aOldAnimationLayers.data( ), m_LatestRecord.m_AnimationLayers.data( ), sizeof( C_AnimationLayer ) * ANIMATION_LAYER_COUNT );
		std::memcpy( aOldPoseParameters.data( ), m_LatestRecord.m_PoseParameters.data( ), sizeof( float_t ) * 24 );

		// rotate player and setup safe matricies
		//onetap v2 crap basicly if you wanna do v4 ResolveLayer crap use brain
		// LODWORD(PlayBackRateDelta4) = COERCE_UNSIGNED_INT(*pRecord + 0x194) - *(pRecord + 0x484));//otv4 4 layer
		for ( int32_t i = -1; i < 2; i++ )
		{

			this->UpdatePlayerAnimations( pPlayer, m_LatestRecord, m_PreviousRecord, i, true );

			// setup safe matrix
			g_BoneManager->BuildMatrix( pPlayer, m_LatestRecord.m_Matricies[ i + 2 ].data( ), true );

			// restore animation data
			std::memcpy( pPlayer->GetAnimationState( ), &AnimationState_NULL, sizeof( C_CSGOPlayerAnimationState ) );
			std::memcpy( pPlayer->GetAnimationLayers( ), aOldAnimationLayers.data( ), sizeof( C_AnimationLayer ) * ANIMATION_LAYER_COUNT );
			std::memcpy( pPlayer->GetBaseAnimating( )->m_aPoseParameters( ).data( ), aOldPoseParameters.data( ), sizeof( float_t ) * 24 );
		}

		// main anim update without player rotation cuz idont wanna give u guys my resolver
		this->UpdatePlayerAnimations( pPlayer, m_LatestRecord, m_PreviousRecord, 0, false );
		
		std::memcpy( pPlayer->GetAnimationLayers( ), m_LatestRecord.m_AnimationLayers.data( ), sizeof( C_AnimationLayer ) * ANIMATION_LAYER_COUNT );
		std::memcpy( m_LatestRecord.m_PoseParameters.data( ), pPlayer->GetBaseAnimating( )->m_aPoseParameters( ).data( ), sizeof( float_t ) * 24 );
		
		g_BoneManager->BuildMatrix( pPlayer, m_LatestRecord.m_Matricies[ 0 ].data( ), false );
		
		std::memcpy( pPlayer->m_CachedBoneData( ).Base( ), m_LatestRecord.m_Matricies[ 0 ].data( ), sizeof( matrix3x4_t ) * pPlayer->m_CachedBoneData( ).Count( ) );
		std::memcpy( g_PlayerData[ iPlayerIndex ].m_aBoneArray.data( ), pPlayer->m_CachedBoneData( ).Base( ), sizeof( matrix3x4_t ) * pPlayer->m_CachedBoneData( ).Count( ) );
	}
	while ( iPlayerIndex < 65 );
}

void C_AnimSync::UpdatePlayerAnimations( C_BasePlayer* pPlayer, C_LagRecord m_LatestRecord, C_LagRecord m_PreviousRecord, int32_t iRotateSide, bool bRotatePlayer )
{
	float_t flCurTime = g_GlobalVars->m_flCurTime;
	float_t flRealTime = g_GlobalVars->m_flRealTime;
	int32_t iFrameCount = g_GlobalVars->m_iFrameCount;
	int32_t iTickCount = g_GlobalVars->m_iTickCount;
	float_t flFrameTime = g_GlobalVars->m_flFrameTime;
	float_t flAbsFrameTime = g_GlobalVars->m_flAbsFrameTime;
	float_t flInterpolation = g_GlobalVars->m_flInterpolationAmount;

	float_t flDuckAmount = pPlayer->m_flDuckAmount( );
	int32_t iEFlags = pPlayer->m_iEFlags( );
	QAngle angEyeAngles = pPlayer->GetCSPlayer( )->m_angEyeAngles( );
	float_t flLowerBodyYaw = pPlayer->GetCSPlayer( )->m_flLowerBodyYaw( );
	
	float_t flFeetCycle = pPlayer->GetAnimationState( )->m_flFeetCycle;
	float_t flFeetWeight = pPlayer->GetAnimationState( )->m_flFeetWeight;

	pPlayer->m_iEFlags( ) &= ~EFL_DIRTY_ABSVELOCITY;
	pPlayer->m_fFlags( ) = m_LatestRecord.m_Flags;
	pPlayer->m_vecVelocity( ) = m_LatestRecord.m_Velocity;
	pPlayer->m_vecAbsVelocity( ) = m_LatestRecord.m_Velocity;

	if (LagRecord.m_SimulationTime - (LagRecord.m_AnimationLayers.at(ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL).m_flCycle / LagRecord.m_AnimationLayers.at(ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL).m_flPlaybackRate)
		>= pPlayer->m_flOldSimulationTime())
		LagRecord.m_Flags |= FL_ONGROUND;

	float_t flWeight = (1.0f - LagRecord.m_AnimationLayers.at(11).m_flWeight) / 2.8571432;
	if (flWeight > 0.0f)
	{
		float_t flSpeed = (flWeight + 0.55f) * pPlayer->GetMaxPlayerSpeed();
		if (flSpeed > 0.0f && LagRecord.m_Velocity.Length() > 0.0f)
			LagRecord.m_Velocity /= LagRecord.m_Velocity.Length() / flSpeed;
	}
	else if (!(LagRecord.m_Flags & FL_ONGROUND))
	{
		float_t flCurrentVelocityDirection = Math::NormalizeAngle(RAD2DEG(atan2(LagRecord.m_Velocity.x, LagRecord.m_Velocity.y)));
		float_t flPreviousVelocityDirection = Math::NormalizeAngle(RAD2DEG(atan2(g_PlayerData[pPlayer->EntIndex()].m_PreviousRecord.m_Velocity.x, g_PlayerData[pPlayer->EntIndex()].m_PreviousRecord.m_Velocity.y)));
		float_t flAverageVelocityDirection = DEG2RAD(Math::NormalizeAngle(flCurrentVelocityDirection + ((flCurrentVelocityDirection - flPreviousVelocityDirection) * 0.5f)));

		float_t flDirectionCos = cos(flAverageVelocityDirection);
		float_t flDirectionSin = sin(flAverageVelocityDirection);

		if (LagRecord.m_Velocity.Length2D() > 0.0f)
		{
			LagRecord.m_Velocity.x /= LagRecord.m_Velocity.Length2D();
			LagRecord.m_Velocity.y /= LagRecord.m_Velocity.Length2D();
		}

		if (flDirectionCos > 0.0f)
			LagRecord.m_Velocity.x *= flDirectionCos;

		if (flDirectionSin > 0.0f)
			LagRecord.m_Velocity.y *= flDirectionSin;

		LagRecord.m_Velocity.z -= g_Globals.m_ConVars.m_SvGravity->GetFloat() * TICKS_TO_TIME(LagRecord.m_ChokedTicks) * 0.5f;
	}

	LagRecord.m_DuckAmount = pPlayer->m_flDuckAmount();
	if (g_PlayerData[pPlayer->EntIndex()].m_bHasPreviousRecord)
		LagRecord.m_DuckPerTick = (LagRecord.m_DuckAmount - g_PlayerData[pPlayer->EntIndex()].m_PreviousRecord.m_DuckAmount) / LagRecord.m_ChokedTicks;

	pPlayer->SetAbsOrigin( m_LatestRecord.m_Origin );
	if ( g_PlayerData[ pPlayer->EntIndex( ) ].m_bHasPreviousRecord )
	{
		pPlayer->GetAnimationState( )->m_iStrafeSequence = m_PreviousRecord.m_AnimationLayers.at( ANIMATION_LAYER_MOVEMENT_STRAFECHANGE ).m_nSequence;
		pPlayer->GetAnimationState( )->m_flStrafeCycle = m_PreviousRecord.m_AnimationLayers.at( ANIMATION_LAYER_MOVEMENT_STRAFECHANGE ).m_flCycle;
		pPlayer->GetAnimationState( )->m_flStrafeWeight = m_PreviousRecord.m_AnimationLayers.at( ANIMATION_LAYER_MOVEMENT_STRAFECHANGE ).m_flWeight;
		pPlayer->GetAnimationState( )->m_flFeetCycle = m_PreviousRecord.m_AnimationLayers.at( ANIMATION_LAYER_MOVEMENT_MOVE ).m_flCycle;
		pPlayer->GetAnimationState( )->m_flFeetWeight = m_PreviousRecord.m_AnimationLayers.at( ANIMATION_LAYER_MOVEMENT_MOVE ).m_flWeight;
		pPlayer->GetAnimationState( )->m_flLeanYaw = m_PreviousRecord.m_AnimationLayers.at( ANIMATION_LAYER_LEAN ).m_flWeight;

		std::memcpy( pPlayer->GetAnimationLayers( ), m_PreviousRecord.m_AnimationLayers.data( ), sizeof( C_AnimationLayer ) * 13 );
	}
	else
	{
		pPlayer->GetAnimationState( )->m_iStrafeSequence = m_LatestRecord.m_AnimationLayers.at( ANIMATION_LAYER_MOVEMENT_STRAFECHANGE ).m_nSequence;
		pPlayer->GetAnimationState( )->m_flStrafeCycle = m_LatestRecord.m_AnimationLayers.at( ANIMATION_LAYER_MOVEMENT_STRAFECHANGE ).m_flCycle;
		pPlayer->GetAnimationState( )->m_flStrafeWeight = m_LatestRecord.m_AnimationLayers.at( ANIMATION_LAYER_MOVEMENT_STRAFECHANGE ).m_flWeight;
		pPlayer->GetAnimationState( )->m_flFeetCycle = m_LatestRecord.m_AnimationLayers.at( ANIMATION_LAYER_MOVEMENT_MOVE ).m_flCycle;
		pPlayer->GetAnimationState( )->m_flFeetWeight = m_LatestRecord.m_AnimationLayers.at( ANIMATION_LAYER_MOVEMENT_MOVE ).m_flWeight;
		pPlayer->GetAnimationState( )->m_flLeanYaw = m_LatestRecord.m_AnimationLayers.at( ANIMATION_LAYER_LEAN ).m_flWeight;

		pPlayer->GetAnimationState( )->m_flLastClientSideAnimationUpdateTime = m_LatestRecord.m_SimulationTime - TICKS_TO_TIME( m_LatestRecord.m_ChokedTicks );
		if ( m_LatestRecord.m_Flags & FL_ONGROUND )
		{
			pPlayer->GetAnimationState( )->m_bOnGround = true;
			pPlayer->GetAnimationState( )->m_bInHitGroundAnimation = false;
		}

		pPlayer->GetAnimationState( )->m_flTotalTimeInAir = 0.0f;
	}

	if ( m_LatestRecord.m_ChokedTicks <= 1 )
	{
		g_GlobalVars->m_flCurTime = m_LatestRecord.m_SimulationTime;
		g_GlobalVars->m_flRealTime = m_LatestRecord.m_SimulationTime;
		g_GlobalVars->m_flFrameTime = g_GlobalVars->m_flIntervalPerTick;
		g_GlobalVars->m_flAbsFrameTime = g_GlobalVars->m_flIntervalPerTick;
		g_GlobalVars->m_iFrameCount = TIME_TO_TICKS( m_LatestRecord.m_SimulationTime );
		g_GlobalVars->m_iTickCount = TIME_TO_TICKS( m_LatestRecord.m_SimulationTime );
		g_GlobalVars->m_flInterpolationAmount = 0.0f;
	
		if ( pPlayer->GetAnimationState( )->m_iLastClientSideAnimationUpdateFramecount >= g_GlobalVars->m_iTickCount )
			pPlayer->GetAnimationState( )->m_iLastClientSideAnimationUpdateFramecount = g_GlobalVars->m_iFrameCount - 1;

		if ( bRotatePlayer )
		{
			float_t flEyeYaw = 0.0f;
			if ( iRotateSide < 0 )
				flEyeYaw = m_LatestRecord.m_EyeAngles.yaw - 60.0f;
			else if ( iRotateSide > 0 )
				flEyeYaw = m_LatestRecord.m_EyeAngles.yaw + 60.0f;
			else
				flEyeYaw = m_LatestRecord.m_EyeAngles.yaw;
		
			pPlayer->GetAnimationState( )->m_flGoalFeetYaw = Math::NormalizeAngle( flEyeYaw );
		}

		pPlayer->GetCSPlayer( )->m_angEyeAngles( ) = m_LatestRecord.m_EyeAngles;
		pPlayer->GetCSPlayer( )->m_flLowerBodyYaw( ) = m_LatestRecord.m_LowerBodyYaw;
		pPlayer->m_flDuckAmount( ) = m_LatestRecord.m_DuckAmount;

		for ( int i = 0; i < 13; i++ )
			pPlayer->GetAnimationLayers( )[ i ].m_pOwner = pPlayer;
		
		bool bClientSideAnimation = pPlayer->GetBaseAnimating( )->m_bClientSideAnimation( );
		g_Globals.m_AnimationData.m_bUpdateClientSideAnimation = pPlayer->GetBaseAnimating( )->m_bClientSideAnimation( ) = true;
		pPlayer->UpdateClientSideAnimation( pPlayer );
		g_Globals.m_AnimationData.m_bUpdateClientSideAnimation = false;
		pPlayer->GetBaseAnimating( )->m_bClientSideAnimation( ) = bClientSideAnimation;

		g_GlobalVars->m_flCurTime = flCurTime;
		g_GlobalVars->m_flRealTime = flRealTime;
		g_GlobalVars->m_flFrameTime = flFrameTime;
		g_GlobalVars->m_flAbsFrameTime = flAbsFrameTime;
		g_GlobalVars->m_iFrameCount = iFrameCount;
		g_GlobalVars->m_iTickCount = iTickCount;
		g_GlobalVars->m_flInterpolationAmount = flInterpolation;
	}
	else
	{
		int32_t iSimulationTick = 1;
		do
		{
			float_t flSimulationTime = pPlayer->m_flOldSimulationTime( ) + TICKS_TO_TIME( iSimulationTick );
			if ( g_PlayerData[ pPlayer->EntIndex( ) ].m_bHasPreviousRecord )
				flSimulationTime = g_PlayerData[ pPlayer->EntIndex( ) ].m_PreviousRecord.m_SimulationTime + TICKS_TO_TIME( iSimulationTick );

			g_GlobalVars->m_flCurTime = flSimulationTime;
			g_GlobalVars->m_flRealTime = flSimulationTime;
			g_GlobalVars->m_flFrameTime = g_GlobalVars->m_flIntervalPerTick;
			g_GlobalVars->m_flAbsFrameTime = g_GlobalVars->m_flIntervalPerTick;
			g_GlobalVars->m_iFrameCount = TIME_TO_TICKS( flSimulationTime );
			g_GlobalVars->m_iTickCount = TIME_TO_TICKS( flSimulationTime );
			g_GlobalVars->m_flInterpolationAmount = 0.0f;
	
			if ( pPlayer->GetAnimationState( )->m_iLastClientSideAnimationUpdateFramecount >= g_GlobalVars->m_iTickCount )
				pPlayer->GetAnimationState( )->m_iLastClientSideAnimationUpdateFramecount = g_GlobalVars->m_iFrameCount - 1;

			if ( bRotatePlayer )
			{
				float_t flEyeYaw = 0.0f;
				if ( iRotateSide < 0 )
					flEyeYaw = m_LatestRecord.m_EyeAngles.yaw - 60.0f;
				else if ( iRotateSide > 0 )
					flEyeYaw = m_LatestRecord.m_EyeAngles.yaw + 60.0f;
				else
					flEyeYaw = m_LatestRecord.m_EyeAngles.yaw;
		
				pPlayer->GetAnimationState( )->m_flGoalFeetYaw = Math::NormalizeAngle( flEyeYaw );
			}

			if ( m_LatestRecord.m_DuckPerTick )
				pPlayer->m_flDuckAmount( ) += m_LatestRecord.m_DuckPerTick * iSimulationTick;
		
			for ( int i = 0; i < 13; i++ )
				pPlayer->GetAnimationLayers( )[ i ].m_pOwner = pPlayer;

			bool bClientSideAnimation = pPlayer->GetBaseAnimating( )->m_bClientSideAnimation( );
			g_Globals.m_AnimationData.m_bUpdateClientSideAnimation = pPlayer->GetBaseAnimating( )->m_bClientSideAnimation( ) = true;
			pPlayer->UpdateClientSideAnimation( pPlayer );
			g_Globals.m_AnimationData.m_bUpdateClientSideAnimation = false;
			pPlayer->GetBaseAnimating( )->m_bClientSideAnimation( ) = bClientSideAnimation;

			g_GlobalVars->m_flCurTime = flCurTime;
			g_GlobalVars->m_flRealTime = flRealTime;
			g_GlobalVars->m_flFrameTime = flFrameTime;
			g_GlobalVars->m_flAbsFrameTime = flAbsFrameTime;
			g_GlobalVars->m_iFrameCount = iFrameCount;
			g_GlobalVars->m_iTickCount = iTickCount;
			g_GlobalVars->m_flInterpolationAmount = flInterpolation;

			// increase simulation tick
			iSimulationTick++;
		}
		while ( iSimulationTick <= m_LatestRecord.m_ChokedTicks );
	}

	if (bRotatePlayer)
	{
		//basicly like legendware matrix crap //you just need 1 brain cell to fix this :)
		std::memcpy(pRecord.ResolverLayers[iRotateSide], pPlayer->GetAnimationLayers(), sizeof(C_AnimationLayer) * 13);
	}

	g_GlobalVars->m_flCurTime = flCurTime;
	g_GlobalVars->m_flRealTime = flRealTime;
	g_GlobalVars->m_flFrameTime = flFrameTime;
	g_GlobalVars->m_flAbsFrameTime = flAbsFrameTime;
	g_GlobalVars->m_iFrameCount = iFrameCount;
	g_GlobalVars->m_iTickCount = iTickCount;
	g_GlobalVars->m_flInterpolationAmount = flInterpolation;

	pPlayer->GetAnimationState( )->m_flFeetCycle = flFeetCycle;
	pPlayer->GetAnimationState( )->m_flFeetWeight = flFeetWeight;

	pPlayer->GetCSPlayer( )->m_angEyeAngles( ) = angEyeAngles;
	pPlayer->GetCSPlayer( )->m_flLowerBodyYaw( ) = flLowerBodyYaw;
	pPlayer->m_flDuckAmount( ) = flDuckAmount;
	pPlayer->m_iEFlags( ) = iEFlags;

	return pPlayer->InvalidatePhysicsRecursive( ANIMATION_CHANGED );
}

void C_AnimSync::UpdateClientSideAnimations( C_BasePlayer* pPlayer )
{
	pPlayer->SetAbsOrigin( pPlayer->m_vecOrigin( ) );

	std::memcpy( pPlayer->m_CachedBoneData( ).Base( ), g_PlayerData[ pPlayer->EntIndex( ) ].m_aBoneArray.data( ), sizeof( matrix3x4_t ) * pPlayer->m_CachedBoneData( ).Count( ) );
	
	const auto aOldMatrix = pPlayer->GetBoneAccessor( ).GetBoneArrayForWrite( );
	pPlayer->GetBoneAccessor( ).m_aBoneArray = g_PlayerData[ pPlayer->EntIndex( ) ].m_aBoneArray.data( );
	pPlayer->SetupBones_AttachmentHelper( );
	pPlayer->GetBoneAccessor( ).m_aBoneArray = aOldMatrix;
}
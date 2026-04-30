#include "MOBAMMOPlayerController.h"

#include "InputKeyEventArgs.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "MOBAMMOBackendConfig.h"
#include "MOBAMMOGameMode.h"
#include "MOBAMMOGameState.h"
#include "MOBAMMOCharacter.h"
#include "MOBAMMOBackendSubsystem.h"
#include "MOBAMMOPlayerState.h"
#include "MOBAMMOTrainingDummyActor.h"
#include "MOBAMMOTrainingMinionActor.h"
#include "MOBAMMOGameUISubsystem.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"

void AMOBAMMOPlayerController::BeginPlay()
{
	Super::BeginPlay();

	EnsureDefaultInputMapping();
}

void AMOBAMMOPlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);
	EnsureDefaultInputMapping();
	ApplyGameplayInputMode();
}

void AMOBAMMOPlayerController::ApplyGameplayInputMode()
{
	SetShowMouseCursor(true);
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);
	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
}

void AMOBAMMOPlayerController::EnsureDefaultInputMapping()
{
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (UInputMappingContext* DefaultMappingContext = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_Default.IMC_Default")))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void AMOBAMMOPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	UMOBAMMOGameUISubsystem* UISubsystem = nullptr;
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		UISubsystem = GameInstance->GetSubsystem<UMOBAMMOGameUISubsystem>();
	}

	if (UISubsystem && UISubsystem->IsChatOpen())
	{
		if (WasInputKeyJustPressed(EKeys::Escape))
		{
			UISubsystem->CloseChat();
		}
		return;
	}

	if (UISubsystem && UISubsystem->IsPauseMenuVisible())
	{
		if (WasInputKeyJustPressed(EKeys::Escape))
		{
			UISubsystem->TogglePauseMenu();
		}
		return;
	}

	ApplyKeyboardMovementFallback();

	if (AMOBAMMOCharacter* MOBACharacter = Cast<AMOBAMMOCharacter>(GetPawn()))
	{
		if (WasInputKeyJustPressed(EKeys::MouseScrollUp))
		{
			MOBACharacter->AdjustCameraZoom(1.0f);
		}
		if (WasInputKeyJustPressed(EKeys::MouseScrollDown))
		{
			MOBACharacter->AdjustCameraZoom(-1.0f);
		}
	}

	if (WasInputKeyJustPressed(EKeys::One))
	{
		TriggerDebugDamage();
	}
	if (WasInputKeyJustPressed(EKeys::Two))
	{
		TriggerDebugHeal();
	}
	if (WasInputKeyJustPressed(EKeys::Three))
	{
		TriggerDebugSpendMana();
	}
	if (WasInputKeyJustPressed(EKeys::Four))
	{
		TriggerDebugRestoreMana();
	}
	if (WasInputKeyJustPressed(EKeys::Five))
	{
		TriggerDebugRespawn();
	}
	if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		TriggerSelectLookTarget(false);
	}
	if (WasInputKeyJustPressed(EKeys::RightMouseButton))
	{
		TriggerPrimaryAttack();
	}
	if (WasInputKeyJustPressed(EKeys::E))
	{
		TriggerSelectLookTarget(true);
	}
	if (WasInputKeyJustPressed(EKeys::I))
	{
		if (UISubsystem)
		{
			UISubsystem->ToggleInventory();
		}
	}
	if (WasInputKeyJustPressed(EKeys::Eight))
	{
		TriggerDebugGrantItem();
	}
	if (WasInputKeyJustPressed(EKeys::Nine))
	{
		TriggerUseInventoryConsumable();
	}
	if (WasInputKeyJustPressed(EKeys::Zero))
	{
		TriggerToggleInventoryEquipment();
	}
	if (WasInputKeyJustPressed(EKeys::Six))
	{
		TriggerCycleDebugTarget();
	}
	if (WasInputKeyJustPressed(EKeys::Seven))
	{
		TriggerClearDebugTarget();
	}
	if (WasInputKeyJustPressed(EKeys::Escape))
	{
		if (UISubsystem)
		{
			UISubsystem->TogglePauseMenu();
		}
	}
	if (WasInputKeyJustPressed(EKeys::Enter))
	{
		TriggerOpenChat();
	}
	if (WasInputKeyJustPressed(EKeys::V))
	{
		TriggerToggleVendor();
	}
	// F1-F4: spend a skill point on abilities 1-4
	if (WasInputKeyJustPressed(EKeys::F1)) { TriggerSpendSkillPoint(0); }
	if (WasInputKeyJustPressed(EKeys::F2)) { TriggerSpendSkillPoint(1); }
	if (WasInputKeyJustPressed(EKeys::F3)) { TriggerSpendSkillPoint(2); }
	if (WasInputKeyJustPressed(EKeys::F4)) { TriggerSpendSkillPoint(3); }
	if (WasInputKeyJustPressed(EKeys::R))
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UMOBAMMOBackendSubsystem* BackendSubsystem = GameInstance->GetSubsystem<UMOBAMMOBackendSubsystem>())
			{
				const AMOBAMMOPlayerState* MOBAPlayerState = GetPlayerState<AMOBAMMOPlayerState>();
				const bool bReplicatedSessionInvalid = MOBAPlayerState && MOBAPlayerState->GetPersistenceStatus() == TEXT("SessionInvalid");
				const bool bCanReconnect = BackendSubsystem->GetSessionStatus() == TEXT("ReconnectRequired") || bReplicatedSessionInvalid;
				UE_LOG(LogTemp, Log, TEXT("[Backend] Reconnect key pressed. SessionStatus=%s PersistenceStatus=%s CanReconnect=%s"),
					*BackendSubsystem->GetSessionStatus(),
					MOBAPlayerState ? *MOBAPlayerState->GetPersistenceStatus() : TEXT("<no player state>"),
					bCanReconnect ? TEXT("true") : TEXT("false"));
				if (bCanReconnect)
				{
					BackendSubsystem->ReconnectCurrentSession(this);
				}
			}
		}
	}
}

void AMOBAMMOPlayerController::ApplyKeyboardMovementFallback()
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn || !ControlledPawn->GetPendingMovementInputVector().IsNearlyZero())
	{
		return;
	}

	const AMOBAMMOPlayerState* MOBAPlayerState = GetPlayerState<AMOBAMMOPlayerState>();
	if (!MOBAPlayerState
		|| MOBAPlayerState->GetSessionId().TrimStartAndEnd().IsEmpty()
		|| MOBAPlayerState->GetCharacterId().TrimStartAndEnd().IsEmpty())
	{
		return;
	}

	const float ForwardValue = (IsInputKeyDown(EKeys::W) ? 1.0f : 0.0f) + (IsInputKeyDown(EKeys::S) ? -1.0f : 0.0f);
	const float RightValue = (IsInputKeyDown(EKeys::D) ? 1.0f : 0.0f) + (IsInputKeyDown(EKeys::A) ? -1.0f : 0.0f);
	if (FMath::IsNearlyZero(ForwardValue) && FMath::IsNearlyZero(RightValue))
	{
		return;
	}

	const FRotator YawRotation(0.0f, GetControlRotation().Yaw, 0.0f);
	if (!FMath::IsNearlyZero(ForwardValue))
	{
		ControlledPawn->AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), ForwardValue);
	}
	if (!FMath::IsNearlyZero(RightValue))
	{
		ControlledPawn->AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), RightValue);
	}
}

FString AMOBAMMOPlayerController::GetSelectedTargetDisplayText() const
{
	const AMOBAMMOPlayerState* MOBAPlayerState = GetPlayerState<AMOBAMMOPlayerState>();
	if (!MOBAPlayerState)
	{
		return TEXT("No target");
	}

	const FString SelectedTargetName = MOBAPlayerState->GetSelectedTargetName();
	return SelectedTargetName.IsEmpty() ? TEXT("No target") : FString::Printf(TEXT("Target %s"), *SelectedTargetName);
}

void AMOBAMMOPlayerController::TriggerDebugDamage()
{
	if (HasAuthority())
	{
		ServerTriggerDebugDamage_Implementation();
		return;
	}

	ServerTriggerDebugDamage();
}

void AMOBAMMOPlayerController::TriggerDebugHeal()
{
	if (HasAuthority())
	{
		ServerTriggerDebugHeal_Implementation();
		return;
	}

	ServerTriggerDebugHeal();
}

void AMOBAMMOPlayerController::TriggerDebugSpendMana()
{
	if (HasAuthority())
	{
		ServerTriggerDebugSpendMana_Implementation();
		return;
	}

	ServerTriggerDebugSpendMana();
}

void AMOBAMMOPlayerController::TriggerDebugRestoreMana()
{
	if (HasAuthority())
	{
		ServerTriggerDebugRestoreMana_Implementation();
		return;
	}

	ServerTriggerDebugRestoreMana();
}

void AMOBAMMOPlayerController::TriggerDebugRespawn()
{
	if (HasAuthority())
	{
		ServerTriggerDebugRespawn_Implementation();
		return;
	}

	ServerTriggerDebugRespawn();
}

void AMOBAMMOPlayerController::TriggerDebugGrantItem()
{
	if (HasAuthority())
	{
		ServerTriggerDebugGrantItem_Implementation();
		return;
	}

	ServerTriggerDebugGrantItem();
}

void AMOBAMMOPlayerController::TriggerPrimaryAttack()
{
	if (AMOBAMMOCharacter* MOBACharacter = Cast<AMOBAMMOCharacter>(GetPawn()))
	{
		MOBACharacter->PlayPrimaryAttackAnimation();
	}

	TriggerSelectLookTarget(true);
	TriggerDebugDamage();
}

void AMOBAMMOPlayerController::TriggerUseInventoryConsumable()
{
	if (HasAuthority())
	{
		ServerUseInventoryConsumable_Implementation();
		return;
	}

	ServerUseInventoryConsumable();
}

void AMOBAMMOPlayerController::TriggerToggleInventoryEquipment()
{
	if (HasAuthority())
	{
		ServerToggleInventoryEquipment_Implementation();
		return;
	}

	ServerToggleInventoryEquipment();
}

void AMOBAMMOPlayerController::TriggerSelectLookTarget(bool bFallbackToTrainingDummy)
{
	if (TrySelectTargetFromView())
	{
		return;
	}

	if (bFallbackToTrainingDummy)
	{
		SelectTrainingMinionTarget();
	}
}

void AMOBAMMOPlayerController::TriggerCycleDebugTarget()
{
	CycleToNextDebugTarget();
}

void AMOBAMMOPlayerController::TriggerClearDebugTarget()
{
	if (HasAuthority())
	{
		ServerClearDebugTarget_Implementation();
		return;
	}

	ServerClearDebugTarget();
}

void AMOBAMMOPlayerController::CycleToNextDebugTarget()
{
	const AGameStateBase* BaseGameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!BaseGameState)
	{
		return;
	}

	const AMOBAMMOPlayerState* LocalPlayerState = GetPlayerState<AMOBAMMOPlayerState>();
	const FString CurrentTargetId = LocalPlayerState ? LocalPlayerState->GetSelectedTargetCharacterId() : FString();

	TArray<AMOBAMMOPlayerState*> Candidates;
	for (APlayerState* IteratedPlayerState : BaseGameState->PlayerArray)
	{
		AMOBAMMOPlayerState* CandidateState = Cast<AMOBAMMOPlayerState>(IteratedPlayerState);
		if (!CandidateState || CandidateState == LocalPlayerState)
		{
			continue;
		}

		Candidates.Add(CandidateState);
	}

	if (Candidates.IsEmpty())
	{
		if (CurrentTargetId == AMOBAMMOTrainingMinionActor::GetTrainingMinionCharacterId())
		{
			SelectTrainingDummyTarget();
		}
		else
		{
			SelectTrainingMinionTarget();
		}
		return;
	}

	int32 NextIndex = 0;
	if (!CurrentTargetId.IsEmpty())
	{
		const int32 CurrentIndex = Candidates.IndexOfByPredicate([&CurrentTargetId](const AMOBAMMOPlayerState* Candidate)
		{
			return Candidate && Candidate->GetCharacterId() == CurrentTargetId;
		});

		if (CurrentIndex != INDEX_NONE)
		{
			NextIndex = (CurrentIndex + 1) % Candidates.Num();
		}
	}

	AMOBAMMOPlayerState* NextTargetState = Candidates[NextIndex];
	if (HasAuthority())
	{
		ServerSetDebugTarget_Implementation(NextTargetState);
		return;
	}

	ServerSetDebugTarget(NextTargetState);
}

void AMOBAMMOPlayerController::SelectTrainingDummyTarget()
{
	if (HasAuthority())
	{
		ServerSetTrainingDummyTarget_Implementation();
		return;
	}

	ServerSetTrainingDummyTarget();
}

void AMOBAMMOPlayerController::SelectTrainingMinionTarget()
{
	if (HasAuthority())
	{
		ServerSetTrainingMinionTarget_Implementation();
		return;
	}

	ServerSetTrainingMinionTarget();
}

bool AMOBAMMOPlayerController::TrySelectTargetFromView()
{
	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;
	GetViewportSize(ViewportSizeX, ViewportSizeY);
	if (ViewportSizeX <= 0 || ViewportSizeY <= 0)
	{
		return false;
	}

	FVector WorldOrigin;
	FVector WorldDirection;
	if (!DeprojectScreenPositionToWorld(ViewportSizeX * 0.5f, ViewportSizeY * 0.5f, WorldOrigin, WorldDirection))
	{
		return false;
	}

	const FVector TraceEnd = WorldOrigin + (WorldDirection * 3500.0f);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MOBAMMOSelectTargetFromView), true);
	QueryParams.AddIgnoredActor(GetPawn());

	FHitResult Hit;
	if (!GetWorld() || !GetWorld()->LineTraceSingleByChannel(Hit, WorldOrigin, TraceEnd, ECC_Visibility, QueryParams))
	{
		return false;
	}

	if (Hit.GetActor() && Hit.GetActor()->IsA<AMOBAMMOTrainingDummyActor>())
	{
		SelectTrainingDummyTarget();
		return true;
	}

	if (Hit.GetActor() && Hit.GetActor()->IsA<AMOBAMMOTrainingMinionActor>())
	{
		SelectTrainingMinionTarget();
		return true;
	}

	return false;
}

void AMOBAMMOPlayerController::ServerTriggerDebugDamage_Implementation()
{
	if (AMOBAMMOGameMode* MOBAGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AMOBAMMOGameMode>() : nullptr)
	{
		MOBAGameMode->CastDebugDamageSpell(this);
	}
}

void AMOBAMMOPlayerController::ServerTriggerDebugHeal_Implementation()
{
	if (AMOBAMMOGameMode* MOBAGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AMOBAMMOGameMode>() : nullptr)
	{
		MOBAGameMode->CastDebugHealSpell(this);
	}
}

void AMOBAMMOPlayerController::ServerTriggerDebugSpendMana_Implementation()
{
	if (AMOBAMMOGameMode* MOBAGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AMOBAMMOGameMode>() : nullptr)
	{
		MOBAGameMode->CastDebugDrainSpell(this);
	}
}

void AMOBAMMOPlayerController::ServerTriggerDebugRestoreMana_Implementation()
{
	if (AMOBAMMOGameMode* MOBAGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AMOBAMMOGameMode>() : nullptr)
	{
		MOBAGameMode->TriggerDebugManaRestore(this);
	}
}

void AMOBAMMOPlayerController::ServerTriggerDebugRespawn_Implementation()
{
	if (AMOBAMMOGameMode* MOBAGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AMOBAMMOGameMode>() : nullptr)
	{
		MOBAGameMode->RespawnPlayer(this);
	}
}

void AMOBAMMOPlayerController::ServerTriggerDebugGrantItem_Implementation()
{
	static const TArray<FString> DebugItemCycle = {
		TEXT("health_potion_small"),
		TEXT("mana_potion_small"),
		TEXT("sparring_token"),
		TEXT("iron_sword"),
		TEXT("crystal_staff"),
		TEXT("leather_helm"),
		TEXT("chain_body"),
		TEXT("swift_boots"),
		TEXT("mana_ring"),
	};

	if (AMOBAMMOGameMode* MOBAGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AMOBAMMOGameMode>() : nullptr)
	{
		MOBAGameMode->CastDebugGrantItem(this, DebugItemCycle[DebugItemCycleIndex % DebugItemCycle.Num()], 1);
		++DebugItemCycleIndex;
	}
}

void AMOBAMMOPlayerController::ServerUseInventoryConsumable_Implementation()
{
	if (AMOBAMMOGameMode* MOBAGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AMOBAMMOGameMode>() : nullptr)
	{
		MOBAGameMode->UseFirstInventoryConsumable(this);
	}
}

void AMOBAMMOPlayerController::ServerToggleInventoryEquipment_Implementation()
{
	if (AMOBAMMOGameMode* MOBAGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AMOBAMMOGameMode>() : nullptr)
	{
		MOBAGameMode->ToggleEquipFirstInventoryItem(this);
	}
}

void AMOBAMMOPlayerController::TriggerOpenChat()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UMOBAMMOGameUISubsystem* UISubsystem = GameInstance->GetSubsystem<UMOBAMMOGameUISubsystem>())
		{
			UISubsystem->OpenChat();
		}
	}
}

void AMOBAMMOPlayerController::TriggerToggleVendor()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UMOBAMMOGameUISubsystem* UISubsystem = GameInstance->GetSubsystem<UMOBAMMOGameUISubsystem>())
		{
			UISubsystem->ToggleVendor();
		}
	}
}

void AMOBAMMOPlayerController::RequestPurchaseFromVendor(const FString& ItemId)
{
	if (HasAuthority())
	{
		ServerPurchaseFromVendor_Implementation(ItemId);
		return;
	}

	ServerPurchaseFromVendor(ItemId);
}

// ─── GM command helpers ─────────────────────────────────────────────────────

static void GMBroadcast(UWorld* World, const FString& Msg)
{
	if (AMOBAMMOGameState* GS = World ? World->GetGameState<AMOBAMMOGameState>() : nullptr)
	{
		GS->PushChatMessage(TEXT("SERVER"), Msg);
	}
}

static AMOBAMMOPlayerState* FindPlayerStateByName(UWorld* World, const FString& Name)
{
	if (!World) return nullptr;
	const AMOBAMMOGameState* GS = World->GetGameState<AMOBAMMOGameState>();
	if (!GS) return nullptr;
	for (APlayerState* RawPS : GS->PlayerArray)
	{
		AMOBAMMOPlayerState* PS = Cast<AMOBAMMOPlayerState>(RawPS);
		if (PS && (PS->GetCharacterName().Equals(Name, ESearchCase::IgnoreCase)
		        || PS->GetPlayerName().Equals(Name, ESearchCase::IgnoreCase)))
		{
			return PS;
		}
	}
	return nullptr;
}

void AMOBAMMOPlayerController::ExecuteGMCommand(const FString& Command, const TArray<FString>& Args)
{
	UWorld* World = GetWorld();
	AMOBAMMOGameMode* GameMode = World ? World->GetAuthGameMode<AMOBAMMOGameMode>() : nullptr;
	const AMOBAMMOPlayerState* PS = GetPlayerState<AMOBAMMOPlayerState>();
	if (!PS || !World) return;

	const FString SenderName = PS->GetCharacterName().IsEmpty() ? PS->GetPlayerName() : PS->GetCharacterName();

	// ── /help ──────────────────────────────────────────────────────────────
	if (Command == TEXT("help"))
	{
		GMBroadcast(World, TEXT("GM Commands: /grant <item> [qty]  /grantall <item> [qty]  /heal [all|@name]  /kill [all|@name]  /broadcast <msg>  /level <n>  /revive [all]  /gold <amount> [@name]"));
		return;
	}

	// ── /broadcast <msg> ───────────────────────────────────────────────────
	if (Command == TEXT("broadcast") || Command == TEXT("bc"))
	{
		if (Args.IsEmpty()) { GMBroadcast(World, TEXT("[GM] Usage: /broadcast <message>")); return; }
		FString Msg;
		for (int32 i = 0; i < Args.Num(); ++i) { if (i > 0) Msg += TEXT(" "); Msg += Args[i]; }
		GMBroadcast(World, FString::Printf(TEXT("[ANNOUNCE] %s"), *Msg));
		UE_LOG(LogTemp, Log, TEXT("[GM] %s broadcast: %s"), *SenderName, *Msg);
		return;
	}

	// ── /grant <itemId> [qty] ──────────────────────────────────────────────
	if (Command == TEXT("grant"))
	{
		if (Args.IsEmpty()) { GMBroadcast(World, TEXT("[GM] Usage: /grant <itemId> [qty]")); return; }
		const FString ItemId = Args[0];
		const int32 Qty = Args.IsValidIndex(1) ? FMath::Clamp(FCString::Atoi(*Args[1]), 1, 99) : 1;
		if (GameMode)
		{
			const bool bOk = GameMode->CastDebugGrantItem(this, ItemId, Qty);
			GMBroadcast(World, bOk
				? FString::Printf(TEXT("[GM] Granted %s x%d to %s"), *ItemId, Qty, *SenderName)
				: FString::Printf(TEXT("[GM] Grant failed: unknown item '%s'"), *ItemId));
		}
		return;
	}

	// ── /grantall <itemId> [qty] ───────────────────────────────────────────
	if (Command == TEXT("grantall"))
	{
		if (Args.IsEmpty()) { GMBroadcast(World, TEXT("[GM] Usage: /grantall <itemId> [qty]")); return; }
		const FString ItemId = Args[0];
		const int32 Qty = Args.IsValidIndex(1) ? FMath::Clamp(FCString::Atoi(*Args[1]), 1, 99) : 1;
		if (GameMode)
		{
			int32 Count = 0;
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				if (APlayerController* TargetPC = It->Get())
				{
					if (GameMode->CastDebugGrantItem(TargetPC, ItemId, Qty)) ++Count;
				}
			}
			GMBroadcast(World, FString::Printf(TEXT("[GM] Granted %s x%d to %d player(s)"), *ItemId, Qty, Count));
		}
		return;
	}

	// ── /heal [all | @name] ────────────────────────────────────────────────
	if (Command == TEXT("heal"))
	{
		const FString Target = Args.IsEmpty() ? TEXT("") : Args[0].ToLower();
		if (GameMode)
		{
			if (Target == TEXT("all"))
			{
				for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
				{
					GameMode->HealPlayer(It->Get(), 99999.0f);
				}
				GMBroadcast(World, TEXT("[GM] Full healed all players"));
			}
			else if (Target.StartsWith(TEXT("@")))
			{
				const FString Name = Target.RightChop(1);
				if (AMOBAMMOPlayerState* TargetPS = FindPlayerStateByName(World, Name))
				{
					if (AController* TC = TargetPS->GetOwningController())
					{
						GameMode->HealPlayer(TC, 99999.0f);
						GMBroadcast(World, FString::Printf(TEXT("[GM] Full healed %s"), *Name));
					}
				}
				else { GMBroadcast(World, FString::Printf(TEXT("[GM] Player '%s' not found"), *Name)); }
			}
			else
			{
				GameMode->HealPlayer(this, 99999.0f);
				GMBroadcast(World, FString::Printf(TEXT("[GM] Full healed %s"), *SenderName));
			}
		}
		return;
	}

	// ── /kill [all | @name] ────────────────────────────────────────────────
	if (Command == TEXT("kill"))
	{
		const FString Target = Args.IsEmpty() ? TEXT("") : Args[0].ToLower();
		if (GameMode)
		{
			if (Target == TEXT("all"))
			{
				for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
				{
					GameMode->ApplyDamageToPlayer(It->Get(), 99999.0f);
				}
				GMBroadcast(World, TEXT("[GM] Killed all players"));
			}
			else if (Target.StartsWith(TEXT("@")))
			{
				const FString Name = Target.RightChop(1);
				if (AMOBAMMOPlayerState* TargetPS = FindPlayerStateByName(World, Name))
				{
					if (AController* TC = TargetPS->GetOwningController())
					{
						GameMode->ApplyDamageToPlayer(TC, 99999.0f);
						GMBroadcast(World, FString::Printf(TEXT("[GM] Killed %s"), *Name));
					}
				}
				else { GMBroadcast(World, FString::Printf(TEXT("[GM] Player '%s' not found"), *Name)); }
			}
			else
			{
				GameMode->ApplyDamageToPlayer(this, 99999.0f);
				GMBroadcast(World, FString::Printf(TEXT("[GM] Killed %s"), *SenderName));
			}
		}
		return;
	}

	// ── /revive [all | @name] ──────────────────────────────────────────────
	if (Command == TEXT("revive"))
	{
		const FString Target = Args.IsEmpty() ? TEXT("") : Args[0].ToLower();
		if (GameMode)
		{
			if (Target == TEXT("all"))
			{
				int32 Count = 0;
				for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
				{
					if (GameMode->RespawnPlayer(It->Get())) ++Count;
				}
				GMBroadcast(World, FString::Printf(TEXT("[GM] Revived %d player(s)"), Count));
			}
			else
			{
				const FString Name = Target.StartsWith(TEXT("@")) ? Target.RightChop(1) : Target;
				AMOBAMMOPlayerState* TargetPS = Name.IsEmpty()
					? GetPlayerState<AMOBAMMOPlayerState>()
					: FindPlayerStateByName(World, Name);
				if (TargetPS)
				{
					if (AController* TC = TargetPS->GetOwningController())
					{
						GameMode->RespawnPlayer(TC);
						GMBroadcast(World, FString::Printf(TEXT("[GM] Revived %s"), *TargetPS->GetCharacterName()));
					}
				}
				else { GMBroadcast(World, TEXT("[GM] Target not found")); }
			}
		}
		return;
	}

	// ── /level <n> ─────────────────────────────────────────────────────────
	if (Command == TEXT("level"))
	{
		if (Args.IsEmpty()) { GMBroadcast(World, TEXT("[GM] Usage: /level <1-99>")); return; }
		const int32 NewLevel = FMath::Clamp(FCString::Atoi(*Args[0]), 1, 99);
		if (AMOBAMMOPlayerState* MyPS = GetPlayerState<AMOBAMMOPlayerState>())
		{
			// Level is stored in CharacterLevel — update via InitializeAttributes path
			// For now set directly since there's no SetLevel API
			MyPS->ApplySessionIdentity(
				MyPS->GetAccountId(), MyPS->GetCharacterId(), MyPS->GetSessionId(),
				MyPS->GetCharacterName(), MyPS->GetClassId(), NewLevel);
			GMBroadcast(World, FString::Printf(TEXT("[GM] %s set to Level %d"), *SenderName, NewLevel));
		}
		return;
	}

	// ── /gold <amount> [@name] ─────────────────────────────────────────────
	if (Command == TEXT("gold"))
	{
		if (Args.IsEmpty())
		{
			GMBroadcast(World, TEXT("[GM] Usage: /gold <amount> [@name]  (use negative amount to deduct)"));
			return;
		}
		const int32 Amount = FCString::Atoi(*Args[0]);
		if (Amount == 0)
		{
			GMBroadcast(World, TEXT("[GM] /gold amount must be non-zero."));
			return;
		}

		AMOBAMMOPlayerState* TargetPS = GetPlayerState<AMOBAMMOPlayerState>();
		if (Args.IsValidIndex(1))
		{
			FString Name = Args[1];
			if (Name.StartsWith(TEXT("@"))) { Name.RemoveAt(0); }
			TargetPS = FindPlayerStateByName(World, Name);
			if (!TargetPS)
			{
				GMBroadcast(World, FString::Printf(TEXT("[GM] /gold: no player named '%s'"), *Name));
				return;
			}
		}

		const FString TargetName = TargetPS->GetCharacterName().IsEmpty() ? TargetPS->GetPlayerName() : TargetPS->GetCharacterName();
		if (Amount > 0)
		{
			const int32 Granted = TargetPS->GrantGold(Amount);
			GMBroadcast(World, FString::Printf(TEXT("[GM] +%d gold to %s (now %d)"), Granted, *TargetName, TargetPS->GetGold()));
		}
		else
		{
			const int32 Deduct = -Amount;
			const bool bOk = TargetPS->SpendGold(Deduct);
			GMBroadcast(World, bOk
				? FString::Printf(TEXT("[GM] -%d gold from %s (now %d)"), Deduct, *TargetName, TargetPS->GetGold())
				: FString::Printf(TEXT("[GM] /gold: %s only has %d (cannot deduct %d)"), *TargetName, TargetPS->GetGold(), Deduct));
		}
		return;
	}

	// ── Unknown ────────────────────────────────────────────────────────────
	GMBroadcast(World, FString::Printf(TEXT("[GM] Unknown command: /%s  — type /help"), *Command));
}

// ─── Main chat message handler ───────────────────────────────────────────────

void AMOBAMMOPlayerController::ServerSendChatMessage_Implementation(const FString& Message)
{
	const FString Trimmed = Message.TrimStartAndEnd().Left(200);
	if (Trimmed.IsEmpty()) return;

	const AMOBAMMOPlayerState* PS = GetPlayerState<AMOBAMMOPlayerState>();
	if (!PS || PS->GetSessionId().TrimStartAndEnd().IsEmpty()) return;

	// ── GM command path (/command [args…]) ──────────────────────────────
	if (Trimmed.StartsWith(TEXT("/")))
	{
		const bool bIsAdmin = GetDefault<UMOBAMMOBackendConfig>()->IsAdminAccount(PS->GetAccountId());
		if (!bIsAdmin)
		{
			// Silently ignore or give a hint visible only to sender via local chat
			if (AMOBAMMOGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMOBAMMOGameState>() : nullptr)
			{
				const FString HintSender = PS->GetCharacterName().IsEmpty() ? PS->GetPlayerName() : PS->GetCharacterName();
				GS->PushChatMessage(HintSender, TEXT("(No GM access)"));
			}
			return;
		}

		// Parse: /command arg1 arg2 …
		TArray<FString> Tokens;
		Trimmed.RightChop(1).ParseIntoArrayWS(Tokens);
		if (Tokens.IsEmpty()) return;

		const FString Cmd = Tokens[0].ToLower();
		Tokens.RemoveAt(0);

		UE_LOG(LogTemp, Log, TEXT("[GM] %s executed: /%s (%d args)"),
			*PS->GetAccountId(), *Cmd, Tokens.Num());

		ExecuteGMCommand(Cmd, Tokens);
		return;
	}

	// ── Regular chat ────────────────────────────────────────────────────
	const FString SenderName = PS->GetCharacterName().IsEmpty() ? PS->GetPlayerName() : PS->GetCharacterName();
	if (AMOBAMMOGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AMOBAMMOGameState>() : nullptr)
	{
		GameState->PushChatMessage(SenderName, Trimmed);
	}
}

void AMOBAMMOPlayerController::ServerSetDebugTarget_Implementation(AMOBAMMOPlayerState* NewTargetPlayerState)
{
	if (AMOBAMMOPlayerState* MOBAPlayerState = GetPlayerState<AMOBAMMOPlayerState>())
	{
		if (NewTargetPlayerState)
		{
			MOBAPlayerState->SetSelectedTargetIdentity(NewTargetPlayerState->GetCharacterId(), NewTargetPlayerState->GetCharacterName());
			if (AMOBAMMOGameState* MOBAGameState = GetWorld() ? GetWorld()->GetGameState<AMOBAMMOGameState>() : nullptr)
			{
				const FString CombatEntry = FString::Printf(TEXT("%s selected %s as target."), *MOBAPlayerState->GetPlayerName(), *NewTargetPlayerState->GetCharacterName());
				MOBAGameState->SetLastCombatLog(CombatEntry);
				MOBAGameState->PushCombatFeedEntry(CombatEntry);
			}
			return;
		}

		MOBAPlayerState->ClearSelectedTarget();
		if (AMOBAMMOGameState* MOBAGameState = GetWorld() ? GetWorld()->GetGameState<AMOBAMMOGameState>() : nullptr)
		{
			const FString CombatEntry = FString::Printf(TEXT("%s cleared target selection."), *MOBAPlayerState->GetPlayerName());
			MOBAGameState->SetLastCombatLog(CombatEntry);
			MOBAGameState->PushCombatFeedEntry(CombatEntry);
		}
	}
}

void AMOBAMMOPlayerController::ServerClearDebugTarget_Implementation()
{
	if (AMOBAMMOPlayerState* MOBAPlayerState = GetPlayerState<AMOBAMMOPlayerState>())
	{
		MOBAPlayerState->ClearSelectedTarget();
	}
}

void AMOBAMMOPlayerController::ServerSetTrainingDummyTarget_Implementation()
{
	const AMOBAMMOGameState* MOBAGameState = GetWorld() ? GetWorld()->GetGameState<AMOBAMMOGameState>() : nullptr;
	if (!MOBAGameState)
	{
		return;
	}

	if (AMOBAMMOPlayerState* MOBAPlayerState = GetPlayerState<AMOBAMMOPlayerState>())
	{
		MOBAPlayerState->SetSelectedTargetIdentity(MOBAGameState->GetTrainingDummyCharacterId(), MOBAGameState->GetTrainingDummyName());
		if (AMOBAMMOGameState* MutableGameState = GetWorld() ? GetWorld()->GetGameState<AMOBAMMOGameState>() : nullptr)
		{
			const FString CombatEntry = FString::Printf(
				TEXT("%s selected %s. Press 1 for Arc Burst or 3 for Drain Pulse."),
				*MOBAPlayerState->GetPlayerName(),
				*MOBAGameState->GetTrainingDummyName()
			);
			MutableGameState->SetLastCombatLog(CombatEntry);
			MutableGameState->PushCombatFeedEntry(CombatEntry);
		}
	}
}

void AMOBAMMOPlayerController::ServerSetTrainingMinionTarget_Implementation()
{
	if (AMOBAMMOPlayerState* MOBAPlayerState = GetPlayerState<AMOBAMMOPlayerState>())
	{
		MOBAPlayerState->SetSelectedTargetIdentity(
			AMOBAMMOTrainingMinionActor::GetTrainingMinionCharacterId(),
			AMOBAMMOTrainingMinionActor::GetTrainingMinionName()
		);

		if (AMOBAMMOGameState* MutableGameState = GetWorld() ? GetWorld()->GetGameState<AMOBAMMOGameState>() : nullptr)
		{
			const FString CombatEntry = FString::Printf(
				TEXT("%s selected %s. Press 1 for Arc Burst."),
				*MOBAPlayerState->GetPlayerName(),
				*AMOBAMMOTrainingMinionActor::GetTrainingMinionName()
			);
			MutableGameState->SetLastCombatLog(CombatEntry);
			MutableGameState->PushCombatFeedEntry(CombatEntry);
		}
	}
}

void AMOBAMMOPlayerController::ServerPurchaseFromVendor_Implementation(const FString& ItemId)
{
	if (AMOBAMMOGameMode* MOBAGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AMOBAMMOGameMode>() : nullptr)
	{
		MOBAGameMode->HandleVendorPurchase(this, ItemId);
	}
}

void AMOBAMMOPlayerController::TriggerSpendSkillPoint(int32 SlotIndex)
{
	if (HasAuthority())
	{
		ServerSpendSkillPoint_Implementation(SlotIndex);
	}
	else
	{
		ServerSpendSkillPoint(SlotIndex);
	}
}

void AMOBAMMOPlayerController::RequestSpendSkillPoint(int32 AbilitySlotIndex)
{
	TriggerSpendSkillPoint(AbilitySlotIndex);
}

void AMOBAMMOPlayerController::ServerSpendSkillPoint_Implementation(int32 AbilitySlotIndex)
{
	if (AMOBAMMOGameMode* MOBAGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AMOBAMMOGameMode>() : nullptr)
	{
		MOBAGameMode->HandleSpendSkillPoint(this, AbilitySlotIndex);
	}
}

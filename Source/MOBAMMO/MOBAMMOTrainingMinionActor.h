#pragma once

#include "CoreMinimal.h"
#include "MOBAMMOAICharacter.h"

#include "MOBAMMOTrainingMinionActor.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UHealthComponent;

UCLASS()
class MOBAMMO_API AMOBAMMOTrainingMinionActor : public AMOBAMMOAICharacter
{
    GENERATED_BODY()

public:
    AMOBAMMOTrainingMinionActor();

    virtual void Tick(float DeltaSeconds) override;
    virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    static const FString& GetTrainingMinionCharacterId();
    static const FString& GetTrainingMinionName();

    void SetAggroTarget(AActor* TargetActor, const FString& TargetName, float DurationSeconds);

    float GetCurrentHealth() const;
    float GetMaxHealth() const;
    bool IsAlive() const;

protected:
    virtual void BeginPlay() override;
    virtual void OnDeath(UHealthComponent* HealthComp, AActor* InstigatorActor) override;

private:
    void UpdatePresentation(float DeltaSeconds);
    void UpdatePatrol(float DeltaSeconds);
    void FaceTextTowardLocalCamera() const;
    void ShowFloatingFeedback(float DamageAmount);
    bool IsAggroActive() const;

    UPROPERTY(VisibleAnywhere, Category="MOBAMMO|Training")
    TObjectPtr<UStaticMeshComponent> BodyMesh;

    UPROPERTY(VisibleAnywhere, Category="MOBAMMO|Training")
    TObjectPtr<UStaticMeshComponent> TargetRingMesh;

    UPROPERTY(VisibleAnywhere, Category="MOBAMMO|Training")
    TObjectPtr<UTextRenderComponent> NameplateText;

    UPROPERTY(VisibleAnywhere, Category="MOBAMMO|Training")
    TObjectPtr<UTextRenderComponent> FloatingFeedbackText;

    FVector SpawnLocation = FVector::ZeroVector;
    FVector PatrolOffset = FVector(0.0f, 220.0f, 0.0f);
    FVector BodyBaseScale = FVector(0.55f, 0.55f, 1.25f);
    FVector RingBaseScale = FVector(1.85f, 1.85f, 0.035f);
    TWeakObjectPtr<AActor> AggroTargetActor;

    UPROPERTY(Replicated)
    FString AggroTargetName;

    UPROPERTY(Replicated)
    float AggroEndServerTime = 0.0f;

    float AggroChaseSpeed = 260.0f;
    float AggroDesiredDistance = 380.0f;
    float PatrolTime = 0.0f;
    float LastObservedHealth = -1.0f;
    float HitPulseRemaining = 0.0f;
    float FloatingFeedbackRemaining = 0.0f;
};

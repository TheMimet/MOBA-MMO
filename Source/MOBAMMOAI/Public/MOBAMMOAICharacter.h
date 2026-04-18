#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MOBAMMOAICharacter.generated.h"

class UHealthComponent;

UCLASS()
class MOBAMMOAI_API AMOBAMMOAICharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AMOBAMMOAICharacter();

    UFUNCTION(BlueprintCallable, Category="Health")
    UHealthComponent* GetHealthComponent() const { return HealthComponent; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI")
    class UBehaviorTree* BehaviorTree;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Health")
    UHealthComponent* HealthComponent;

    UFUNCTION()
    virtual void OnDeath(UHealthComponent* HealthComp, AActor* InstigatorActor);
};

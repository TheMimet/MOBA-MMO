#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "MOBAMMOAIController.generated.h"

class UBehaviorTree;
class UBlackboardComponent;
class UBehaviorTreeComponent;

UCLASS()
class MOBAMMOAI_API AMOBAMMOAIController : public AAIController
{
    GENERATED_BODY()

public:
    AMOBAMMOAIController();

    virtual void OnPossess(APawn* InPawn) override;

    UFUNCTION(BlueprintCallable, Category="AI")
    void SetAIState(FName NewStateName, uint8 StateValue);

protected:
    UPROPERTY(Transient)
    UBehaviorTreeComponent* BehaviorTreeComponent;

    UPROPERTY(Transient)
    UBlackboardComponent* BlackboardComponent;
};

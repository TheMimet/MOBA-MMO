#include "MOBAMMOAIController.h"
#include "MOBAMMOAICharacter.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

AMOBAMMOAIController::AMOBAMMOAIController()
{
    BehaviorTreeComponent = CreateDefaultSubobject<UBehaviorTreeComponent>(TEXT("BehaviorTreeComponent"));
    BlackboardComponent = CreateDefaultSubobject<UBlackboardComponent>(TEXT("BlackboardComponent"));
}

void AMOBAMMOAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    AMOBAMMOAICharacter* AICharacter = Cast<AMOBAMMOAICharacter>(InPawn);
    if (AICharacter && AICharacter->BehaviorTree)
    {
        if (AICharacter->BehaviorTree->BlackboardAsset)
        {
            BlackboardComponent->InitializeBlackboard(*(AICharacter->BehaviorTree->BlackboardAsset));
        }

        BehaviorTreeComponent->StartTree(*(AICharacter->BehaviorTree));
    }
}

void AMOBAMMOAIController::SetAIState(FName NewStateName, uint8 StateValue)
{
    if (BlackboardComponent)
    {
        BlackboardComponent->SetValueAsEnum(NewStateName, StateValue);
    }
}

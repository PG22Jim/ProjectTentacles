// Copyright (C) The Tentacle Zone 2023. All Rights Reserved.


#include "Characters/Base/Widget_EnemyBaseUserWidget.h"

void UWidget_EnemyBaseUserWidget::ShowIndicator()
{
	IndicationImage->SetVisibility(ESlateVisibility::Visible);
}

void UWidget_EnemyBaseUserWidget::UnShowIndicator()
{
	IndicationImage->SetVisibility(ESlateVisibility::Hidden);
	StopAllAnimations();
}

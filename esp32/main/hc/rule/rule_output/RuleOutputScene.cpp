#include "RuleOutputScene.h"
#include "Log.h"

RuleOutputScene::RuleOutputScene(Scene *scene)
{
	this->scene = scene;
}

RuleOutputScene::~RuleOutputScene()
{
	LOGI("~RuleOutputScene");
}

void RuleOutputScene::RunOutput()
{
	if (scene)
	{
		scene->Do(false);
	}
}

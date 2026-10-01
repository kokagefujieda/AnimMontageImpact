using UnrealBuildTool;

public class AnimMontageImpact : ModuleRules
{
	public AnimMontageImpact(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// 公開ヘッダー (UActorComponent / UAnimNotifyState / FHitResult 等) が使う型のため Public にする
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine"
			}
		);
	}
}

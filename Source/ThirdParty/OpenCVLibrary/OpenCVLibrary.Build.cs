using UnrealBuildTool;
using System.IO;

public class OpenCVLibrary : ModuleRules
{
    public OpenCVLibrary(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        
        Type = ModuleType.External;

        string OpenCVPath = Path.Combine(ModuleDirectory);

        PublicIncludePaths.Add(Path.Combine(OpenCVPath, "include"));

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicAdditionalLibraries.Add(Path.Combine(OpenCVPath, "lib", "Win64", "opencv_world4120.lib"));

            RuntimeDependencies.Add("$(BinaryOutputDir)/opencv_world4120.dll",Path.Combine(OpenCVPath, "bin", "Win64", "opencv_world4120.dll"));
            RuntimeDependencies.Add("$(ProjectDir)/Binaries/Win64/opencv_world4120.dll", Path.Combine(OpenCVPath, "bin", "Win64", "opencv_world4120.dll"));

            PublicDelayLoadDLLs.Add("opencv_world4120.dll");
        }
    }
}   
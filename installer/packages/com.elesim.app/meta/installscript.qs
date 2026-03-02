function Component() {}

Component.prototype.createOperations = function() {
    component.createOperations();

    if (systemInfo.productType === "windows") {
        // Start menu shortcut
        component.addOperation("CreateShortcut",
            "@TargetDir@/EleSim.exe",
            "@StartMenuDir@/EleSim.lnk",
            "workingDirectory=@TargetDir@",
            "description=Electronic Circuit Simulator");

        // Desktop shortcut
        component.addOperation("CreateShortcut",
            "@TargetDir@/EleSim.exe",
            "@DesktopDir@/EleSim.lnk",
            "workingDirectory=@TargetDir@",
            "description=Electronic Circuit Simulator");
    }
}

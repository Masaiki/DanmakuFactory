project(DanmakuFactoryCLI LANGUAGES C)
set(CMAKE_C_STANDARD 11)

ADD_EXECUTABLE(DanmakuFactoryCLI
    "String/DanmakuFactoryString.c"
    "List/DanmakuFactoryList.c"
    "Config/Config.c"
    "TemplateFile/TemplateFile.c"
    "AssFile/AssStringProcessing.c"
    "AssFile/AssFile.c"
    "XmlFile.c"
    "JsonFile.c"
    "main.c"
)
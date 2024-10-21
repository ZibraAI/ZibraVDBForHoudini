# Contributing to ZibraVDB for Houdini

## Contributor License Agreement

By contributing to this project, you agree that any intellectual property you contribute will become part of the repository and be distributed under the [ZibraVDB for Houdini License](LICENSE). Furthermore, you agree to transfer all rights, title, and interest in any contributions to ZibraAI Inc., the project maintainer, ensuring that ZibraAI Inc. holds the full rights to use, modify, and distribute the contributions as part of this project or in any derivative work. You also acknowledge that the license governing the contributed code may be changed by ZibraAI Inc. in the future, at its discretion.

## C++ Code Guidelines

- Naming
    - Abbreviations are always uppercase regardless of other rules
        - e.g.
            - Local variable: `UAVs`, but not `uavs`
            - Class name: `ZibraHUD`, but not `ZibraHud`
            - Global varible: `g_CSInitParticles`, but not `g_CsInitParticles`
    - All classes should be named starting with uppercase letter using camel case
        - e.g. `ZibraLiquid`, but not `zibraLiquid`, `Zibra_Liquid` or `ZIBRA_LIQUID`
    - Functions/Class Methods should be named starting with uppercase letter using camel case
        - e.g. `InitializeResources`, but not `initializeResources`, `Initialize_Resources` or `INITIALIZE_RESOURSES`
    - Class attributes
        - Non static attributes prefixed with `m_` (member) and then name starting with uppercase letter using camel case
            - e.g. `m_DeviceContext`, but not `DeviceContext`, `m_deviceContext`, `m_Device_Context`
        - Static attributes prefixed with `ms_` (member static) and then named starting with uppercase letter using camel case
            - e.g. `ms_BufferSize`, but not `m_BufferSize`, `s_BufferSize`, `BufferSize`, `ms_bufferSize`
    - Struct attributes
        - In case of small structures you can use lowercase names
            - e.g.

                ```cpp
                struct Vector 
                {
                    float x;
                    float y;
                    float z;
                };
                ```

        - In any other case - naming is same as for classes (or even better use a class)
    - Local variables should be named starting with lowercase letter using camel case
        - e.g. `textBuffer`, but not `TextBuffer`, `Text_Buffer` or `TEXT_BUFFER`
    - Global variables
        - Try to avoid them, prefer static class attributes
        - If you have to use global variable you should prefix it with `g_` (global) and then name starting with uppercase letter using camel case
            - e.g. `g_ShaderBytecode`, but not `ShaderBytecode`, `shaderBytecode` or `ms_ShaderBytecode`
    - Enums
        - Enums and Enum classes should be named starting with uppercase letter using camel case, and also should be in singular form
            - e.g. `MessageType`, but not `MessageTypes`, `MESSAGE_TYPE` or `messageType`
        - Enum values in plain enum should be named starting with enum name followed by `_` followed by the name of specific enum value that start with uppercase letter using camel case
            - e.g. `MessageType_YesNo`, not `MessageTypeYesNo`, `YesNo`, `MessageType_YESNO` or `MessageType_yesno`
        - Enum values in enum classes should be named starting with uppercase letter using camel case
            - e.g. `FatalError`, not `fatalError`, `FATAL_ERROR` or `Fatal_Error`
    - Custom preprocessor defines should always be prefixed with `ZIB_` and named in all uppercase letters with underscore `_` between words
        - e.g. `ZIB_ARR_SIZE`, but not `ARR_SIZE`, `ArrSize`, `arrSize`, `ZIBARRSIZE`, `ZIB_ArrSize`, `zib_arr_size` or `Zib_Arr_Size`
        - Reasoning behind prefixing all macros with `ZIB_` is that macros can't be namespaced or isolated in any way, which may lead to name collision. By prefixing all definitions we eliminate possibility that some 3rd party header will introduce same definitions
    - Typedef/Using follow same rules as underlying type
- OOP
    - Virtual functions
        - When overriding virtual function you must specify `override` or `final`, and not specify `virtual`
            - e.g.
                ```cpp
                void InitializeResources() final;
                ```
                
                but not 
                ```cpp
                virtual void InitializeResources() final;
                ```
                ```cpp
                virtual void InitializeResources();
                ```
                or
                ```cpp
                void InitializeResources();
                ```
            
        - Reasoning behind not writing virtual is that if you already have `override` or `final` it already implies `virtual`
    - When you know that function shouldn't be overriden by more derived classes (or if there should be no more derived classes) you should use `final`           
    - Use proper access specifiers for classes (having everything public in small structs is fine)
        - Public - methods/attributes that will be accessible outiside
            - Only use for interface methods
            - Don't use for any kind of attributes
        - Protected - methods/attributes that will be available to derived class
            - Use for methods/attributes which may be necessary to access from derived classes
        - Private - methods/attributes that will only be available in it's class
            - Use for methods/attributes that derived classes shouldn't access
        - When overriding a function use same access specifier as virtual function in base class
    - Everything inside class that can logically be static should be static
        - e.g. function

            ```cpp
            static string ToUpper(const std::string value);
            ```

            you don't need to access object to change passed string to upper case

        - e.g. member attribute
            
            ```cpp
            static const size_t ms_MaxBufferSize = 65536;
            ```
            
            since all instances of class have same limit
            
    - Everything inside class that can logically be const should be const
        - e.g. function
            
            ```cpp
            size_t GetEffectSize() const
            ```
            
            since it would only return some data already stored inside class attributes, don't need to modify it
            
        - e.g. member attribute
            
            ```cpp
            static const size_t ms_MaxBufferSize = 65536;
            ```
            
            since buffer size limit doesn't change during execution
            
    - Do not use multiple inheritance unless absolutely neccecary
- Code
    - **Do not use `std::map::operator[]`**
        - It can modify the map, and nobody in ZibraAI ever wanted that behaviour yet
        - use `std::map::find()` instead
    - In cpp files, put includes into 3 separate groups, split by empty line:
        - 1st - precompiled header
            - Exactly 1 include here
            - Each cpp file must have precompiled header as first include
        - 2nd - Header corresponding to current cpp file
            - Exactly 1 include here
            - e.g. for `ZibraVDB.cpp` it’s going to be `ZibraVDB.h`
        - 3rd - Everything else
    - Try to use descriptive, not too abreviated names
        - e.g. `EncodedCoordinate` instead of `enccoord`, or `FindFirstCharacter` insead of `strpbrk`
    - If name is not enough to make it clear what function do, what function parameters expected, what is return value, etc. from name, leave a comment explaining that
        - e.g.

            ```cpp
            // Downloads file to disk, creating folder structure if necessary
            bool DownloadFile(const std::string& url, const std::string& filepath);
            ```
            
    - If there's a non-obvious reason for having some piece of code (or not having it), leave a comment why is it there
        - e.g.

            ```cpp
            // 0 index referring to the node's first input
            m_InputSOP = CAST_SOPNODE(getInputFollowingOutputs(0));
            ```
            
    - If some piece of code neeeds improvement and won't get it in current pull request put "// TODO" comment with description of what is needed to do

        - e.g. `// TODO cross-platform support`

    - If there's any other situation where it's makes sense to put a comment, put a comment

    - Don't put comments for code which is easy to understand already

        - e.g.
            
            ```cpp
            // Creates compressor instance
            [[nodiscard]] uint32_t CreateCompressorInstance(const ZCE_CompressionSettings* settings) noexcept;
            ```
            
    - Don't put comments that doesn't explain anything
        - e.g.
            
            ```cpp
            //Set aabb values to int32 min/max values
            CompressionEngine::ZCE_AABB aabb = {std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::max(),
                                    std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::min(),
                                    std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::min()};

            ```
            
    - Prefer giving better names to function, variables, etc. instead of adding a comment about its purpose
        - e.g.

            ```cpp
            bool DownloadFile(const std::string& url, const std::string& filepath);
            ```
            
            Instead of

            ```cpp
            // Downloads file
            bool Get(const std::string& url, const std::string& filepath);
            ```
            
    - Give names to "magic constants" unless it's clear why they are there
        - e.g.
            
            ```cpp
            #define ZIB_MAX_CHANNEL_COUNT 8
            ```
            
            Makes it clear what 8 is, and marks each place where same limit should be used
            
            But
            
            ```cpp
            uint32_t compressorID = 0;
            ```
            
            Perfectly fine, it is just empty value
            
    - Split large functions into smaller functions with descriptive names
    - Don't use structs in case you'll have a lot of logic, only use struct if that struct will primarily store data (and maybe small number of methods).
        - e.g.
            
            ```cpp
            struct Vector 
            {
                float x;
                float y;
                float z;
            };
            ```
            
            And
            
            ```cpp
            class OpenVDBDecoder {
                ...
            ```
            
    - Do not use `using namespace std;` in the outer scope, unless it's for single cpp file where you heavily use std namespace
    - Do not use `auto` unless type it hides is too long to read anyway, or you are using lambdas (which require auto)
        - good example when auto is allowed is iterators, having `auto iterator = ...` is very easy to understand
        - Reasoning for it is that it's not clear what is the underlying type, which make code harder to understand and sometimes introduce unintended consequences (e.g. vector copy)
    - Prefer enum classes, since names for enum values will be better, and you will be able to have names that would otherwise intersect

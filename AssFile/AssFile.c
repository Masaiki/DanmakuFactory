/* MIT License
 * 
 * Copyright (c) 2022 hkm
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "AssFile.h"
#include "AssStringProcessing.h"

static int printStatDataStr(FILE *filePtr, const float startTime, const float endTime, const int posX,
    const int posY, const char *effect, const char *str);
static int printStatDataInt(FILE *filePtr, const float startTime, const float endTime, const int posX,
    const int posY, char *effect, const int data);
static int printMessage(FILE *filePtr,
    int startPosX, int startPosY, int endPosX, int endPosY, float startTime, float endTime,
    int width, int fontSize, char *effect, DANMAKU *message);
static int printTime(FILE *filePtr, float time, const char *endText);
static int getMsgBoxHeight(DANMAKU *message, int fontSize, int width);
static char *getActionStr(char *dstBuff,int shiftX, int shiftY, int startPosX, int startPosY, int endPosX, int endPosY);
static int findMin(float *array, const int numOfLine, const int stopSubScript, const int mode);
static float getEndTime(DANMAKU *danmakuPtr, const int rollTime, const int holdTime);

/* 
 * 读取ass文件到弹幕池
 * 参数：文件名/弹幕池/读取模式/字幕部分输出/时轴偏移/状态
 * 返回值：状态码
 * 个位为函数assFileToDanmaku的返回值
 * 十位为函数readAssFile的返回值
  */
int readAss(const char *const fileName, DANMAKU **danmakuHead, const char *mode, ASSFILE *assSub, const float timeShift, STATUS *const status)
{
    /* 刷新status */
    if (status != NULL)
    {
        status -> function = (void *)readAss;
        status -> completedNum = 0;
        status -> isDone = FALSE;
    }
    
    int returnValue = 0;
    ASSFILE assDanmaku;
    returnValue += readAssFile(&assDanmaku, fileName);

    returnValue *= 10;
    returnValue += assFileToDanmaku(&assDanmaku, danmakuHead, mode, assSub, timeShift, status);
    
    return returnValue;
}

/* 
 * 读取ass文件
 * 参数：文件结构体指针/ass文件名
 * 返回值：
 * 0 正常退出
 * 1 打开文件失败
 * 2 3 4 内存空间分配失败
  */
int readAssFile(ASSFILE *assFile, const char *const fileName)
{
    FILE *fptr;
    if ((fptr = fopen(fileName, "r")) == NULL)
    {
        return 1;
    }
    
    memset(assFile, 0, sizeof(ASSFILE));
    assFile -> stylesNum = 0;
    
    char line[ASS_MAX_LINE_LEN];
    char temp[TEMPBUF_SIZE];
    char mark[MARKSTR_SIZE], lowerMark[MARKSTR_SIZE];
    char *ptr;
    STYLE *newStyle;
    EVENT *newNode, *tailNode = NULL; /* 记录尾节点以提高新建速度 */
    
    /* 
    处理的标记 Title/ScriptType/Collisions/PlayResX/PlayResY/Timer/Style/Dialogue/Comment/
               Picture/Sound/Movie/Command
    其他标记可以加但没有必要 直接忽略行
     */
    while (!feof(fptr))
    {
        /* 读取一行 */
        if (fgets(line, ASS_MAX_LINE_LEN, fptr) == NULL)
        {
            break;
        }
        line[ASS_MAX_LINE_LEN - 1] = '\0';
        if (line[strlen(line)-1] == '\n')
        {
            line[strlen(line)-1] = '\0';
        }
        
        /* 读取标记 */
        ptr = line; 
        strGetLeftPart(mark, &ptr, ':', MARKSTR_SIZE);
        trim(mark);/* 去左右空格 */ 
        toLower(lowerMark, mark);/* 转换为小写 */ 
        
        /* 解析标记 */ 
        if (!strcmp(lowerMark, "dialogue") || !strcmp(lowerMark, "command") || !strcmp(lowerMark, "picture") ||
            !strcmp(lowerMark, "sound")    || !strcmp(lowerMark, "movie")   || !strcmp(lowerMark, "command") )
        {/* 事件标签 */
            /* 新建节点 */
            if ((newNode = (EVENT *)malloc(sizeof(EVENT))) == NULL)
            {
                freeAssFile(assFile);
                return 2;
            }
            
            /* dialogue: 0,0:00:01.60,0:00:06.60,Default,,0000,0000,0000,,{\pos(960,343)}你指尖跃动的电光 */
            /* 标记 */
            strSafeCopy(newNode -> event, mark, ASS_EVENT_TYPE_LEN);    
            
            /* 层 */
            strGetLeftPart(temp, &ptr, ',', TEMPBUF_SIZE);
            newNode -> layer = atoi(temp);
            
            /* 开始时间 */
            strGetLeftPart(temp, &ptr, ',', TEMPBUF_SIZE);
            newNode -> start = timeToFloat(temp);
            
            /* 结束时间 */
            strGetLeftPart(temp, &ptr, ',', TEMPBUF_SIZE);
            newNode -> end = timeToFloat(temp);
            
            /* 样式 */
            strGetLeftPart(temp, &ptr, ',', TEMPBUF_SIZE);
            trim(temp);
            strSafeCopy(newNode -> style, temp, ASS_EVENT_STYLE_LEN);
            
            /* 角色名称 */
            strGetLeftPart(temp, &ptr, ',', TEMPBUF_SIZE);
            trim(temp);
            strSafeCopy(newNode -> name, temp, ASS_EVENT_NAME_LEN);
            
            /* 左边距 */
            strGetLeftPart(temp, &ptr, ',', TEMPBUF_SIZE);
            newNode -> marginL = atoi(temp);
            
            /* 右边距 */
            strGetLeftPart(temp, &ptr, ',', TEMPBUF_SIZE);
            newNode -> marginR = atoi(temp);
            
            /* 垂直边距 */
            strGetLeftPart(temp, &ptr, ',', TEMPBUF_SIZE);
            newNode -> marginV = atoi(temp);
            
            /* 过渡效果 */
            strGetLeftPart(temp, &ptr, ',', TEMPBUF_SIZE);
            trim(temp);
            strSafeCopy(newNode -> effect, temp, ASS_EVENT_EFFECT_LEN);
            
            /* 文本部分 */
            if ((newNode -> text = (char *)malloc((strlen(ptr) + 1) * sizeof(char))) == NULL)
            {
                free(newNode);
                freeAssFile(assFile);
                return 3;
            }
            strcpy(newNode -> text, ptr);
            
            /* 新建链表节点 */
            if (assFile -> events != NULL)
            {/* 链表头不为空 追加节点 */ 
                newNode -> next = NULL;
                tailNode -> next = newNode;
                tailNode = newNode;
            }
            else
            {/* 链表头为空 修改链表头 */
                assFile -> events = newNode;
                newNode -> next = NULL;
                tailNode = newNode;
            }
        }
        else if (!strcmp(lowerMark, "style"))
        {/* 样式 */ 
            /* Style: Default,Microsoft YaHei Light,38,
            &H1EFFFFFF,&H1EFFFFFF,&H1E000000,&H1E6A5149,
            0,0,0,0,100.00,100.00,0.00,0.00,1,0,1,8,0,0,0,1 */
            
            /* 新建数组成员 */ 
            if ((newStyle = (STYLE *)realloc(assFile -> styles, sizeof(STYLE) * (assFile->stylesNum+1))) == NULL)
            {
                freeAssFile(assFile);
                return 4;
            }
            
            /* 名称 */
            strGetLeftPart(temp, &ptr, ',', TEMPBUF_SIZE);
            trim(temp);
            strSafeCopy(newStyle[assFile->stylesNum].name, temp, ASS_STYLE_NAME_LEN);
            
            /* 字体 */
            strGetLeftPart(temp, &ptr, ',', TEMPBUF_SIZE);
            trim(temp);
            strSafeCopy(newStyle[assFile->stylesNum].fontname, temp, ASS_STYLE_FONTNAME_LEN);
            
            /* 字号 */
            strGetLeftPart(temp, &ptr, ',', TEMPBUF_SIZE);
            newStyle[assFile->stylesNum].fontsize = atoi(temp);
            
            /* 主要颜色 */
            strGetLeftPart(temp, &ptr, ',', TEMPBUF_SIZE);
            trim(temp);
            strSafeCopy(newStyle[assFile->stylesNum].primaryColour, temp, ASS_COLOR_LEN);
            
            /* 次要颜色 */
            strGetLeftPart(temp, &ptr, ',', TEMPBUF_SIZE);
            trim(temp);
            strSafeCopy(newStyle[assFile->stylesNum].secondaryColour, temp, ASS_COLOR_LEN);
            
            /* 边框颜色 */
            strGetLeftPart(temp, &ptr, ',', TEMPBUF_SIZE);
            trim(temp);
            strSafeCopy(newStyle[assFile->stylesNum].outlineColor, temp, ASS_COLOR_LEN);
            
            /* 背景颜色 */
            strGetLeftPart(temp, &ptr, ',', TEMPBUF_SIZE);
            trim(temp);
            strSafeCopy(newStyle[assFile->stylesNum].backColour, temp, ASS_COLOR_LEN);
            
            /* 粗体 */
            strGetLeftPart(temp, &ptr, ',', TEMPBUF_SIZE);
            newStyle[assFile->stylesNum].bold = atoi(temp);
            
            /* 斜体 */
            strGetLeftPart(temp, &ptr, ',', TEMPBUF_SIZE);
            newStyle[assFile->stylesNum].italic = atoi(temp);
            
            /* 下划线 */
            strGetLeftPart(temp, &ptr, ',', TEMPBUF_SIZE);
            newStyle[assFile->stylesNum].underline = atoi(temp);
            
            /* 删除线 */
            strGetLeftPart(temp, &ptr, ',', TEMPBUF_SIZE);
            newStyle[assFile->stylesNum].strikeout = atoi(temp);
            
            /* 横向缩放 */
            strGetLeftPart(temp, &ptr, ',', TEMPBUF_SIZE);
            newStyle[assFile->stylesNum].scaleX = atoi(temp);
            
            /* 纵向缩放 */
            strGetLeftPart(temp, &ptr, ',', TEMPBUF_SIZE);
            newStyle[assFile->stylesNum].scaleY = atoi(temp);
            
            /* 字间距 */
            strGetLeftPart(temp, &ptr, ',', TEMPBUF_SIZE);
            newStyle[assFile->stylesNum].spacing = atof(temp);
            
            /* 旋转角度 */
            strGetLeftPart(temp, &ptr, ',', TEMPBUF_SIZE);
            newStyle[assFile->stylesNum].angle = atof(temp);
            
            /* 边框样式 */
            strGetLeftPart(temp, &ptr, ',', TEMPBUF_SIZE);
            newStyle[assFile->stylesNum].borderStyle = atoi(temp);
            
            /* 边框深度 */
            strGetLeftPart(temp, &ptr, ',', TEMPBUF_SIZE);
            newStyle[assFile->stylesNum].outline = atof(temp);
            
            /* 阴影深度 */
            strGetLeftPart(temp, &ptr, ',', TEMPBUF_SIZE);
            newStyle[assFile->stylesNum].shadow = atof(temp);
            
            /* 对齐方式 */
            strGetLeftPart(temp, &ptr, ',', TEMPBUF_SIZE);
            newStyle[assFile->stylesNum].alignment = atoi(temp);
            
            /* 左边距 */
            strGetLeftPart(temp, &ptr, ',', TEMPBUF_SIZE);
            newStyle[assFile->stylesNum].marginL = atoi(temp);
            
            /* 右边距 */
            strGetLeftPart(temp, &ptr, ',', TEMPBUF_SIZE);
            newStyle[assFile->stylesNum].marginR = atoi(temp);
            
            /* 垂直边距 */
            strGetLeftPart(temp, &ptr, ',', TEMPBUF_SIZE);
            newStyle[assFile->stylesNum].marginV = atoi(temp);
            
            /* 编码 */
            newStyle[assFile->stylesNum].encoding = atoi(ptr);
            
            /* 样式数目更新 */
            assFile -> styles = newStyle;
            assFile -> stylesNum += 1;
        }
        else if (!strcmp(lowerMark, "title"))
        {/* 标题 */
            /* Title: test */
            trim(ptr);
            strSafeCopy(assFile->title, ptr, ASS_TITLE_LEN);
        }
        else if (!strcmp(lowerMark, "scripttype"))
        {/* 脚本版本 */
            /* ScriptType: v4.00+ */
            trim(ptr);
            strSafeCopy(assFile->scriptType, ptr, ASS_SCRIPT_TYPE_LEN);
        }
        else if (!strcmp(lowerMark, "collisions"))
        {/* 碰撞决策 */
            /* Collisions: Normal */
            trim(ptr);
            strSafeCopy(assFile->collisions, ptr, ASS_COLLISIONS_LEN);
        }
        else if (!strcmp(lowerMark, "playresx"))
        {/* 分辨率x */
            /* PlayResX: 1920 */
            assFile -> playResX = atoi(ptr);
        }
        else if (!strcmp(lowerMark, "playresy"))
        {/* 分辨率y */
            /* PlayResY: 1080 */
            assFile -> playResY = atoi(ptr);
        }
        else if (!strcmp(lowerMark, "timer"))
        {/* 分辨率y */
            /* Timer: 100.0000 */
            assFile -> timer = atof(ptr);
        }
    }
    
    fclose(fptr);
    return 0;
}

/* 
 * 将 ASSFILE 转换为弹幕 DANMAKU
 * 转换结果包含弹幕部分与字幕部分（如果有的话） 
 * 参数：输入ass文件结构体/输出弹幕链表头指针的指针（空）/输出字幕部分（NULL 不解析）/时轴偏移量
 * 返回值：
 * 0 正常退出
 * 1 2 3 4 5 6 内存空间分配失败
  */
int assFileToDanmaku(ASSFILE *inputSub, DANMAKU **danmakuHead,
                     const char *mode, ASSFILE *outputSub, const float timeShift, STATUS *const status
                    )
{
    int cnt;
    char tempStr[ASS_MAX_LINE_LEN];             /* 临时字符串 */
    char *leftPtr, *rightPtr;                   /* 临时指针 定位字符串左边与右边 */
    EVENT *newEventNode, *tailEventNode;        /* 事件新节点 事件尾节点 */ 
    EVENT *inEventPtr, *outEventPtr;            /* 输入输出事件节点指针 */
    STYLE *inStyleArr, *outStyleArr;            /* 输入输出样式数组 */ 
    DANMAKU *newDanmakuNode, *tailDanmakuNode;  /* 弹幕新节点 弹幕尾节点 */ 
    SPPART *newSpcialPart;                      /* 新建弹幕特殊部分结构体 */ 
    
    inEventPtr = inputSub -> events;
    inStyleArr = inputSub -> styles;
    
    /* 刷新status */
    if (status != NULL)
    {
        status -> function = (void *)assFileToDanmaku;
        status -> completedNum = 0;
        status -> isDone = FALSE;
    }
    
    if (mode[0] == 'n')
    {
        freeList(*danmakuHead);
        *danmakuHead = NULL;
    }
    else if (mode[0] == 'a' && *danmakuHead != NULL)
    {
        tailDanmakuNode = *danmakuHead;
        while (tailDanmakuNode -> next != NULL)
        {
            tailDanmakuNode = tailDanmakuNode -> next;
        }
    }
    
    /* 字幕部分结构体属性拷贝 */
    if (outputSub != NULL)
    {
        outEventPtr = outputSub -> events;
        outStyleArr = outputSub -> styles;
        memset(outputSub, 0, sizeof(ASSFILE));
        
        strSafeCopy(outputSub -> scriptType, "v4.00+", ASS_SCRIPT_TYPE_LEN);
        strSafeCopy(outputSub -> collisions, "Normal", ASS_COLLISIONS_LEN);
        outputSub -> timer = 100.00;
        outputSub -> stylesNum = 0;
        outputSub -> events = NULL;
        
        /* 拷贝样式表 */
        for (cnt = 0; cnt < inputSub -> stylesNum; cnt++)
        {
            if (strstr(inStyleArr[cnt].name, "danmakuFactory_ext_sub") == inStyleArr[cnt].name)
            {
                outputSub -> stylesNum++;
                if ((outStyleArr = (STYLE *)realloc(outStyleArr, sizeof(STYLE) * (outputSub->stylesNum))) == NULL)
                {
                    return 1;
                }
                outStyleArr[outputSub->stylesNum-1] = inStyleArr[cnt];
                
                /* 去除前缀 */
                deStyleNamePrefix(outStyleArr[outputSub->stylesNum-1].name);
            }/* end if */
        }/* end for */
    }
    
    while (inEventPtr != NULL)
    {
        toLower(NULL, inEventPtr -> event);
        toLower(NULL, inEventPtr -> name);
        
        /* 字幕部分 */
        if (outputSub != NULL && strstr(inEventPtr->style, "danmakuFactory_ext_sub") == inEventPtr->style)
        {
            char *textPart;
            
            /* 新建节点 */
            if ((newEventNode = (EVENT *)malloc(sizeof(EVENT))) == NULL)
            {
                freeAssFile(outputSub);
                //TODO:释放弹幕链表
                return 2;
            }
            
            /* 内容拷贝 */
            //TODO:文本内容拷贝
            strSafeCopy(newEventNode -> event, inEventPtr -> event, ASS_EVENT_TYPE_LEN);
            newEventNode -> layer = inEventPtr -> layer;
            newEventNode -> start = inEventPtr -> start;
            newEventNode -> end = inEventPtr -> end;
            strSafeCopy(newEventNode -> style, inEventPtr -> style, ASS_EVENT_STYLE_LEN);
            strSafeCopy(newEventNode -> name, inEventPtr -> name, ASS_EVENT_NAME_LEN);
            newEventNode -> marginL = inEventPtr -> marginL;
            newEventNode -> marginR = inEventPtr -> marginR;
            newEventNode -> marginV = inEventPtr -> marginV;
            strSafeCopy(newEventNode -> effect, inEventPtr -> effect, ASS_EVENT_EFFECT_LEN);
            
            /* 去除前缀 */
            deStyleNamePrefix(newEventNode -> style);
            
            if ((textPart = (char *)malloc(sizeof(char) * (strlen(inEventPtr->text) + 1))) == NULL)
            {
                freeAssFile(outputSub);
                //TODO:释放弹幕链表
                return 3;
            }
            
            strcpy(textPart, inEventPtr->text);
            newEventNode -> text = textPart;
            
            /* 新建链表节点 */
            if (outputSub -> events != NULL)
            {/* 链表头不为空 追加节点 */ 
                newEventNode -> next = NULL;
                tailEventNode -> next = newEventNode;
                tailEventNode = newEventNode;
            }
            else
            {/* 链表头为空 修改链表头 */
                outputSub -> events = newEventNode;
                newEventNode -> next = NULL;
                tailEventNode = newEventNode;
            }
        }
        
        /* 统计部分 */
        if (strstr(inEventPtr->style, "danmakuFactory_stat") == inEventPtr->style)
        {
            /* 直接抛弃 */
            inEventPtr = inEventPtr -> next;
            continue;
        }

        /* 消息框部分 */
        if (strstr(inEventPtr->style, "message_box") == inEventPtr->style)
        {
            /* 暂不支持 直接抛弃 */
            inEventPtr = inEventPtr -> next;
            continue;
        }
        
        /* 弹幕部分 */ 
        if (!strcmp(inEventPtr -> event, "dialogue") || !strcmp(inEventPtr -> event, "comment"))
        {
            if (strlen(inEventPtr -> text) == 0)
            {
                /* 文本部分为空直接抛弃 */
                inEventPtr = inEventPtr -> next;
                continue;
            }
            
            int fontColor = -1;
            int fontSize = 0;
            
            char *textPart, *textPartPtr;
            char codePart[1024], *codePartPtr;
            char singleCode[128], *singleCodePtr;
            
            float existTime = 0.00;
            int moveTime = 0, pauseTime = 0;
            int startX = 0, startY = 0, endX = 0, endY = 0;
            int fadeStart = 0, fadeEnd = 0, frY = 0, frZ = 0;
            char fontName[FONTNAME_LEN];
            
            /* 合法性检查 */
            existTime = inEventPtr->end - inEventPtr->start;
            if (existTime < 0)
            {
                inEventPtr = inEventPtr -> next;
                continue;
            }
            
            memset(fontName, 0, FONTNAME_LEN);
            
            if ((textPart = (char *)malloc(strlen(inEventPtr -> text) * sizeof(char))) == NULL)
            {
                freeAssFile(outputSub);
                //TODO:释放弹幕链表 
                return 4;
            }
            
            memset(textPart, 0, strlen(inEventPtr -> text) * sizeof(char));
            leftPtr = rightPtr = inEventPtr -> text;
            textPartPtr = textPart;
            codePartPtr = codePart;
            
            /* 分割代码和文本部分 */
            while (*rightPtr != '\0')
            {
                /* 
                    匹配规则
                    1.遍历字符串当遇到左花括号时向右寻找匹配右括号
                      如果存在右括号，则左右两括号内部为代码部分
                      如果不存在右括号，则包括左括号本身到字串末尾都是文本部分
                    2.其余内容都是文本部分
                 */
                if (*rightPtr == '{')
                {
                    leftPtr = rightPtr;
                    while (*rightPtr != '}' && *rightPtr != '\0')
                    {
                        rightPtr++;
                    }
                    
                    if (*rightPtr == '\0')
                    {
                        /* 文本部分 */
                        while (leftPtr != rightPtr)
                        {
                            *textPartPtr = *leftPtr;
                            textPartPtr++;
                            leftPtr++;
                        }
                        
                        break;/* 已经读到字串末尾了 */
                    }
                    else
                    {
                        /* 代码部分 */ 
                        leftPtr++;
                        while (leftPtr != rightPtr)
                        {
                            *codePartPtr = *leftPtr;
                            codePartPtr++;
                            leftPtr++;
                        }
                    }
                }
                else
                {
                    /* 文本部分 */ 
                    *textPartPtr = *rightPtr;
                    textPartPtr++;
                }/* end if */
                
                rightPtr++;
            }
            *textPartPtr = '\0';
            *codePartPtr = '\0';

            /* ass转义字符转换 */
            {
                char tempText[MAX_TEXT_LENGTH];
                strSafeCopy(tempText, textPart, MAX_TEXT_LENGTH);
                assEscape(textPart, tempText, MAX_TEXT_LENGTH, ASS_UNESCAPE);
            }
            
            /* 解析代码部分 */
            codePartPtr = strstr(codePart, "\\pos");/* 固定内容 */
            if (codePartPtr != NULL)
            {
                codePartPtr += 4;/* 跳过代码名 */
                /* 命令与左括号之间允许存在任意字符 */
                while (*codePartPtr != '(' && *codePartPtr != '\\' && *codePartPtr != '\0')
                {
                    codePartPtr++;
                }
                
                if (*codePartPtr == '(')
                {
                    codePartPtr++;
                    /* 提取有效字符 */
                    singleCodePtr = singleCode;
                    cnt = 0;
                    while (*codePartPtr != ')' && *codePartPtr != '\\' && *codePartPtr != '\0' && cnt < 127)
                    {
                        if (isDesignatedChar(*codePartPtr, ",-.0123456789") == TRUE)
                        {
                            *singleCodePtr = *codePartPtr;
                            singleCodePtr++;
                            cnt++;
                        }
                        
                        codePartPtr++;
                    }
                    *singleCodePtr = '\0';
                    singleCodePtr = singleCode;
                    
                    startX = endX = atoi(strGetLeftPart(tempStr, &singleCodePtr, ',', 128));
                    startY = endY = atoi(singleCodePtr);
                }
            }
            
            codePartPtr = strstr(codePart, "\\move");/* 内容移动 */
            if (codePartPtr != NULL)
            {
                codePartPtr += 5;/* 跳过代码名 */
                /* 命令与左括号之间允许存在任意字符 */
                while (*codePartPtr != '(' && *codePartPtr != '\\' && *codePartPtr != '\0')
                {
                    codePartPtr++;
                }
                
                if (*codePartPtr == '(')
                {
                    codePartPtr++;
                    /* 提取有效字符 */
                    singleCodePtr = singleCode;
                    cnt = 0;
                    while (*codePartPtr != ')' && *codePartPtr != '\\' && *codePartPtr != '\0' && cnt < 127)
                    {
                        if (isDesignatedChar(*codePartPtr, ",-.0123456789") == TRUE)
                        {
                            *singleCodePtr = *codePartPtr;
                            singleCodePtr++;
                            cnt++;
                        }
                        
                        codePartPtr++;
                    }
                    *singleCodePtr = '\0';
                    singleCodePtr = singleCode;
                    
                    startX = atoi(strGetLeftPart(tempStr, &singleCodePtr, ',', 128));
                    startY = atoi(strGetLeftPart(tempStr, &singleCodePtr, ',', 128));
                    endX = atoi(strGetLeftPart(tempStr, &singleCodePtr, ',', 128));
                    if (strchr(singleCodePtr, ',') == NULL)
                    {
                        endY = atoi(singleCodePtr);
                    }
                    else
                    {
                        endY = atoi(strGetLeftPart(tempStr, &singleCodePtr, ',', 128));
                    }
                    
                    pauseTime = atoi(strGetLeftPart(tempStr, &singleCodePtr, ',', 128));
                    moveTime = atoi(singleCodePtr) - pauseTime;
                }
            }
            
            codePartPtr = strstr(codePart, "\\fs");/* 字号 */
            if (codePartPtr != NULL)
            {
                codePartPtr += 3;/* 跳过代码名 */
                /* 提取有效字符 */
                singleCodePtr = singleCode;
                cnt = 0;
                while (*codePartPtr != '\\' && *codePartPtr != '\0' && cnt < 127)
                {
                    if (isDesignatedChar(*codePartPtr, "0123456789") == TRUE)
                    {
                        *singleCodePtr = *codePartPtr;
                        singleCodePtr++;
                        cnt++;
                    }
                    
                    codePartPtr++;
                }
                *singleCodePtr = '\0';
                singleCodePtr = singleCode;
                
                fontSize = atoi(singleCodePtr);
            }
            
            codePartPtr = strstr(codePart, "\\fn");/* 字体 */
            if (codePartPtr != NULL)
            {
                codePartPtr += 3;/* 跳过代码名 */
                cnt = 0;
                while (*codePartPtr != '\\' && *codePartPtr != '\0' && cnt < FONTNAME_LEN)
                {
                    fontName[cnt] = *codePartPtr;
                    codePartPtr++;
                    cnt++;
                }
                fontName[cnt] = '\0';
            }
            
            codePartPtr = strstr(codePart, "\\c");/* 颜色 */
            if (codePartPtr != NULL)
            {
                codePartPtr += 2;/* 跳过代码名 */ 
                /* 提取有效字符 */
                singleCodePtr = singleCode;
                cnt = 0;
                while (*codePartPtr != '\\' && *codePartPtr != '\0' && cnt < 6)
                {
                    if (isDesignatedChar(*codePartPtr, "0123456789abcdefABCDEF") == TRUE)
                    {
                        *singleCodePtr = *codePartPtr;
                        singleCodePtr++;
                        cnt++;
                    }
                    
                    codePartPtr++;
                }
                
                fontColor = toDecColor(singleCode);
            }
            
            codePartPtr = strstr(codePart, "\\fry");/* Y轴旋转 */
            if (codePartPtr != NULL)
            {
                codePartPtr += 4;/* 跳过代码名 */ 
                /* 提取有效字符 */
                singleCodePtr = singleCode;
                cnt = 0;
                while (*codePartPtr != '\\' && *codePartPtr != '\0' && cnt < 127)
                {
                    if (isDesignatedChar(*codePartPtr, "-0123456789") == TRUE)
                    {
                        *singleCodePtr = *codePartPtr;
                        singleCodePtr++;
                        cnt++;
                    }
                    
                    codePartPtr++;
                }
                *singleCodePtr = '\0';
                singleCodePtr = singleCode;
                
                frY = atoi(singleCodePtr);
            }
            
            codePartPtr = strstr(codePart, "\\frz");/* Z轴旋转 */
            if (codePartPtr != NULL)
            {
                codePartPtr += 4;/* 跳过代码名 */ 
                /* 提取有效字符 */
                singleCodePtr = singleCode;
                cnt = 0;
                while (*codePartPtr != '\\' && *codePartPtr != '\0' && cnt < 127)
                {
                    if (isDesignatedChar(*codePartPtr, "-0123456789") == TRUE)
                    {
                        *singleCodePtr = *codePartPtr;
                        singleCodePtr++;
                        cnt++;
                    }
                    
                    codePartPtr++;
                }
                *singleCodePtr = '\0';
                singleCodePtr = singleCode;
                
                frZ = atoi(singleCodePtr);
            }
            
            codePartPtr = strstr(codePart, "\\fade");/* 淡出淡入 */
            if (codePartPtr != NULL)
            {
                codePartPtr += 5;/* 跳过代码名 */ 
                /* 命令与左括号之间允许存在任意字符 */
                while (*codePartPtr != '(' && *codePartPtr != '\\' && *codePartPtr != '\0')
                {
                    codePartPtr++;
                }
                
                if (*codePartPtr == '(')
                {
                    codePartPtr++;
                    /* 提取有效字符 */
                    singleCodePtr = singleCode;
                    cnt = 0;
                    while (*codePartPtr != ')' && *codePartPtr != '\\' && *codePartPtr != '\0' && cnt < 127)
                    {
                        if (isDesignatedChar(*codePartPtr, ",-0123456789") == TRUE)
                        {
                            *singleCodePtr = *codePartPtr;
                            singleCodePtr++;
                            cnt++;
                        }
                        
                        codePartPtr++;
                    }
                    *singleCodePtr = '\0';
                    singleCodePtr = singleCode;
                    
                    /* 只解析前三个字段 */
                    fadeStart = atoi(strGetLeftPart(tempStr, &singleCodePtr, ',', 128));
                    strGetLeftPart(NULL, &singleCodePtr, ',', 128);
                    fadeEnd = atoi(strGetLeftPart(tempStr, &singleCodePtr, ',', 128));
                }
            }/* 代码解析完成 */
            
            /* 申请一个节点的空间 */
            if ((newDanmakuNode = (DANMAKU *)malloc(sizeof(DANMAKU))) == NULL)
            {
                //TODO:释放弹幕与字幕链表
                //free(textPart);
                return 5;
            }
            newDanmakuNode -> special = NULL; 
            
            /* 
             *+-------------+---------+
             *|  类型名称   |  编号   |
             *+-------------+---------+
             *|  右左滚动   |    1    |
             *+-------------+---------+
             *|  左右滚动   |    2    |
             *+-------------+---------+
             *|  上方固定   |    3    |
             *+-------------+---------+
             *|  下方固定   |    4    |
             *+-------------+---------+
             *| B站特殊弹幕 |    5    |
             *+-------------+---------+
              */
            
            /* 类型判断 */
            if (strcmp(inEventPtr -> style, "R2L") == 0)
            {
                newDanmakuNode -> type = 1;
            }
            else if (strcmp(inEventPtr -> style, "L2R") == 0)
            {
                newDanmakuNode -> type = 2;
            }
            else if (strcmp(inEventPtr -> style, "TOP") == 0)
            {
                newDanmakuNode -> type = 3;
            }
            else if (strcmp(inEventPtr -> style, "BTM") == 0)
            {
                newDanmakuNode -> type = 4;
            }
            else if (strcmp(inEventPtr -> style, "SP") == 0)
            {
                newDanmakuNode -> type = 5;
            }
            else if(moveTime == 0 && pauseTime == 0 &&
                    fadeStart == 0 && fadeEnd == 0  &&
                    frY == 0 && frZ == 0 &&
                    strlen(fontName) == 0
                   )
            {/* 其他程序生成的ass 普通弹幕 */ 
                if (startX == endX && startY == endY && abs(startX - (inputSub->playResX)/2) < 2)
                {/* 固定弹幕 */
                    if (startY < (inputSub->playResY)/2)
                    {/* 顶部固定 */
                        newDanmakuNode -> type = 3;
                    }
                    else
                    {/* 底部固定 */ 
                        newDanmakuNode -> type = 4;
                    }
                }
                else if (startY == endY && startX > inputSub->playResX && endX < 0)
                {/* 右左滚动 */
                    newDanmakuNode -> type = 1;
                }
                else if (startY == endY && endX > inputSub->playResX && startX < 0)
                {/* 左右滚动 */
                    newDanmakuNode -> type = 2;
                }
                else
                {/* 特殊弹幕 */
                    newDanmakuNode -> type = 5;
                }
            }
            else
            {/* 特殊弹幕 */
                newDanmakuNode -> type = 5;
            }
            
            //第三方弹幕正确率测试
            #if FALSE
            {
                static int debug_cnt = 0;
                static int debug_total = 0;
                if (
                    (newDanmakuNode -> type = 1 && strcmp(inEventPtr -> style, "R2L") == 0) ||
                    (newDanmakuNode -> type = 2 && strcmp(inEventPtr -> style, "L2R") == 0) ||
                    (newDanmakuNode -> type = 3 && strcmp(inEventPtr -> style, "TOP") == 0) ||
                    (newDanmakuNode -> type = 4 && strcmp(inEventPtr -> style, "BTM") == 0) ||
                    (newDanmakuNode -> type = 5 && strcmp(inEventPtr -> style, "SP" ) == 0)
                   )
                {
                    //printf("\n>%04d 命中", debug_total); 
                }
                else
                {
                    printf("\n>%04d 未命中", debug_total);
                    printf("#%s# -> %d", inEventPtr -> style, newDanmakuNode -> type);
                    debug_cnt++;
                }
                debug_total++;
                printf("\n 准确率%.2f%% (ERR:%d/%d)\n", 100-(debug_cnt/(float)debug_total*100.00), debug_cnt, debug_total);
            }
            #endif
            //第三方弹幕正确率测试
            
            /* 遍历样式表 寻找匹配的样式 */
            for (cnt = 0; cnt < inputSub->stylesNum; cnt++)
            {
                if (strcmp(inEventPtr->style, inputSub->styles[cnt].name) == 0)
                {
                    break;
                }
            }
            
            if (cnt == inputSub->stylesNum)
            {/* 没有对应样式 抛弃本条 */
                //TODO:抛弃之前检查是否全部属性都有说明（兼容某些不用样式表的转换工具）
                free(newDanmakuNode);
                free(textPart);
                inEventPtr = inEventPtr -> next;
                continue;
            }
            
            /* 写入弹幕数据 */
            /* 字号 */
            if (fontSize == 0)
            {
                newDanmakuNode -> fontSize = 25;
            }
            else
            {
                newDanmakuNode -> fontSize = fontSize - inputSub->styles[cnt].fontsize + 25;
            }
            
            /* 颜色 */
            if (fontColor == -1)
            {
                if (strlen(inputSub->styles[cnt].primaryColour) < 10)
                {/* 如果该字段为空 默认为白 */
                    newDanmakuNode -> color = 16777215;
                }
                else
                {/* +4是为了跳过前面的不透明度字段 */
                    newDanmakuNode -> color = toDecColor(inputSub->styles[cnt].primaryColour + 4);
                }
            }
            else
            {
                newDanmakuNode -> color = fontColor;
            }
            
            newDanmakuNode -> time = inEventPtr -> start + timeShift;/* 开始时间 */
            if (newDanmakuNode -> time < 0)
            {
                newDanmakuNode -> time = 0.00;
            }
            newDanmakuNode -> text = textPart;/* 文本部分 */
            
            /* 特殊弹幕部分 */
            if (IS_SPECIAL(newDanmakuNode))
            {
                if ((newSpcialPart = (SPPART *)malloc(sizeof(SPPART))) == NULL)
                {
                    //TODO:释放弹幕与字幕链表
                    free(newDanmakuNode);
                    free(textPart);
                    return 6;
                }
                newDanmakuNode -> special = newSpcialPart;
                
                newSpcialPart -> existTime = existTime;
                newSpcialPart -> moveTime = moveTime;
                newSpcialPart -> pauseTime = pauseTime;
                newSpcialPart -> startX = fabs(startX / (float)inputSub -> playResX);
                newSpcialPart -> startY = fabs(startY / (float)inputSub -> playResY);
                newSpcialPart -> endX = fabs(endX / (float)inputSub -> playResX);
                newSpcialPart -> endY = fabs(endY / (float)inputSub -> playResY);
                newSpcialPart -> fadeStart = fadeStart;
                newSpcialPart -> fadeEnd = fadeEnd;
                newSpcialPart -> frY = frY;
                newSpcialPart -> frZ = frZ;
                strSafeCopy(newSpcialPart -> fontName, fontName, FONTNAME_LEN);
            }
            
            /* 弹幕链表增加节点 */
            if (*danmakuHead == NULL)
            {/* 修改头节点 */
                newDanmakuNode -> next = NULL;
                tailDanmakuNode = newDanmakuNode;
                *danmakuHead = newDanmakuNode; 
            }
            else
            {
                newDanmakuNode -> next = NULL;
                tailDanmakuNode -> next = newDanmakuNode;
                tailDanmakuNode = newDanmakuNode;
            }
            
            /* 刷新status */
            if (status != NULL)
            {
                (status -> totalNum)++;
            }
            
        }/* end if */
        inEventPtr = inEventPtr -> next;
    }
    
    /* 刷新status */
    if (status != NULL)
    {
        status -> isDone = TRUE;
    }
    
    return 0;
}

/*  
 * 写ass文件
 * 参数：输出文件名/弹幕池/配置信息/字幕部分/状态
 * 返回值：状态码
 * 个位 函数writeAssStatPart的返回值
 * 十位 函数writeAssDanmakuPart的返回值
 * 百位 1 弹幕池为空
  */
int writeAss(const char *const fileName, DANMAKU *danmakuHead,
             const CONFIG config, const ASSFILE *const subPart,
             STATUS *const status
            )
{
    FILE *fptr = fopen(fileName, "w");
    
    int returnValue = 0;
    
    /* 刷新status */
    if (status != NULL)
    {
        status -> function = (void *)writeAss;
        status -> completedNum = 0;
        status -> isDone = FALSE;
    }
    
    if (fptr == NULL)
    {
        return 100;
    }
    
    /* 写info部分 */
    fprintf(fptr, "[Script Info]\n"
                  "ScriptType: v4.00+\n"
                  "Collisions: Normal\n"
                  "PlayResX: %d\n"
                  "PlayResY: %d\n"
                  "Timer: 100.0000\n\n",
            config.resolution.x, config.resolution.y
           );
    
    /* 写styles部分 */ 
    {
        fprintf(fptr, "[V4+ Styles]\n"
                      "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, "
                      "OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, "
                      "ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, "
                      "MarginL, MarginR, MarginV, Encoding\n"
               );
        char hexOpacity[3];
        char primaryColour[ASS_COLOR_LEN];
        int bold = 0;
        toHexOpacity(255 - config.opacity, hexOpacity);
        sprintf(primaryColour, "&H%sFFFFFF", hexOpacity);

        if (config.bold == TRUE)
        {
            bold = 1;
        }
        else
        {
            bold = 0;
        }
        
        
        /* 样式设定 */
        fprintf(fptr, "\nStyle: %s,%s,%d,%s,%s,%s,%s,%d,%d,%d,%d,%.2f,%.2f,%.2f,%.2f,%d,%d,%d,%d,%d,%d,%d,%d",
                     "R2L", config.fontname, config.fontsize, primaryColour, "&H00FFFFFF", "&H00000000", "&H1E6A5149",
                     bold, 0, 0, 0, 100.00, 100.00, 0.00, 0.00, 1, config.outline, config.shadow, 8,
                     0, 0, 0, 1
               );
        
        fprintf(fptr, "\nStyle: %s,%s,%d,%s,%s,%s,%s,%d,%d,%d,%d,%.2f,%.2f,%.2f,%.2f,%d,%d,%d,%d,%d,%d,%d,%d",
                     "L2R", config.fontname, config.fontsize, primaryColour, "&H00FFFFFF", "&H00000000", "&H1E6A5149",
                     bold, 0, 0, 0, 100.00, 100.00, 0.00, 0.00, 1, config.outline, config.shadow, 8,
                     0, 0, 0, 1
               );
        
        fprintf(fptr, "\nStyle: %s,%s,%d,%s,%s,%s,%s,%d,%d,%d,%d,%.2f,%.2f,%.2f,%.2f,%d,%d,%d,%d,%d,%d,%d,%d",
                     "TOP", config.fontname, config.fontsize, primaryColour, "&H00FFFFFF", "&H00000000", "&H1E6A5149",
                     bold, 0, 0, 0, 100.00, 100.00, 0.00, 0.00, 1, config.outline, config.shadow, 8,
                     0, 0, 0, 1
               );
        
        fprintf(fptr, "\nStyle: %s,%s,%d,%s,%s,%s,%s,%d,%d,%d,%d,%.2f,%.2f,%.2f,%.2f,%d,%d,%d,%d,%d,%d,%d,%d",
                     "BTM", config.fontname, config.fontsize, primaryColour, "&H00FFFFFF", "&H00000000", "&H1E6A5149",
                     bold, 0, 0, 0, 100.00, 100.00, 0.00, 0.00, 1, config.outline, config.shadow, 8,
                     0, 0, 0, 1
               );
        
        fprintf(fptr, "\nStyle: %s,%s,%d,%s,%s,%s,%s,%d,%d,%d,%d,%.2f,%.2f,%.2f,%.2f,%d,%d,%d,%d,%d,%d,%d,%d",
                     "SP", config.fontname, config.fontsize, "&H00FFFFFF", "&H00FFFFFF", "&H00000000", "&H1E6A5149",
                     bold, 0, 0, 0, 100.00, 100.00, 0.00, 0.00, 1, config.outline, config.shadow, 7,
                     0, 0, 0, 1
               );
        
        fprintf(fptr, "\nStyle: %s,%s,%d,%s,%s,%s,%s,%d,%d,%d,%d,%.2f,%.2f,%.2f,%.2f,%d,%d,%d,%d,%d,%d,%d,%d",
                     "message_box", config.fontname, config.msgboxFontsize, "&H00FFFFFF", "&H00FFFFFF", "&H00000000", "&H1E6A5149",
                     bold, 0, 0, 0, 100.00, 100.00, 0.00, 0.00, 1, 0, 0, 7,
                     0, 0, 0, 1
               );
        
        if (config.statmode != 0)
        {
            fprintf(fptr, "\nStyle: %s,%s,%d,%s,%s,%s,%s,%d,%d,%d,%d,%.2f,%.2f,%.2f,%.2f,%d,%d,%d,%d,%d,%d,%d,%d",
                     "danmakuFactory_stat", config.fontname, config.fontsize, "&H35FFFFFF", "&H35FFFFFF", "&H35000000", "&H356A5149",
                     0, 0, 0, 0, 100.00, 100.00, 0.00, 0.00, 0, 0, 0, 5,
                     0, 0, 0, 1
                    );
        }
        
        if (subPart != NULL)
        {
            //TODO:写外部字幕部分样式 
        }
    }
    
    /* 写events部分 */  
    fprintf(fptr, "\n\n[Events]\n"
                 "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text");
    
    returnValue *= 10;
    returnValue += writeAssDanmakuPart(fptr, danmakuHead, config, status);
    
    /* 刷新status */
    if (status != NULL)
    {
        status -> function = (void *)writeAssStatPart;
        status -> completedNum = 0;
        status -> isDone = FALSE;
    }
    
    returnValue *= 10;
    returnValue += writeAssStatPart(fptr, danmakuHead,
                                    config.statmode,
                                    config.scrolltime, config.fixtime,
                                    config.density, config.blockmode
                                   );
    
    //TODO:添加函数writeAssSubPart()
    
    fclose(fptr);
    
    return returnValue;
}

/*  
 * 写ass文件styles部分
 * 参数：输出文件指针/样式条数/样式结构体
 * 返回值：空 
  */
void writeAssStylesPart(FILE *opF, const int numOfStyles, STYLE *const styles)
{
    int cnt;
    for (cnt = 0; cnt < numOfStyles; cnt++)
    {
        fprintf(opF, "\nStyle: %s,%s,%d,%s,%s,%s,%s,%d,%d,%d,%d,%.2f,%.2f,%.2f,%.2f,%d,%d,%d,%d,%d,%d,%d,%d",
                    styles[cnt].name, styles[cnt].fontname, styles[cnt].fontsize, styles[cnt].primaryColour,
                    styles[cnt].secondaryColour, styles[cnt].outlineColor, styles[cnt].backColour,
                    styles[cnt].bold, styles[cnt].italic, styles[cnt].underline, styles[cnt].strikeout,
                    (float)styles[cnt].scaleX, (float)styles[cnt].scaleY, styles[cnt].spacing, styles[cnt].angle,
                    styles[cnt].borderStyle, (int)styles[cnt].outline, (int)styles[cnt].shadow, styles[cnt].alignment,
                    styles[cnt].marginL, styles[cnt].marginR, styles[cnt].marginV, styles[cnt].encoding);
    }
}

/* 
 * 将弹幕池写到ass文件 
 * 参数：
 * 输入文件/输出文件/配置项/写状态
 * 返回值：
 * 0 正常退出
 * 1 弹幕池为空
 * 2 文件指针为空
 * 3 4 5 6 7 8 申请内存空间失败
 * 9 写文件时发生错误 
  */
int writeAssDanmakuPart(FILE *opF, DANMAKU *head, CONFIG config, STATUS *const status)
{
    COORDIN resolution = config.resolution;
    const int fontSize = config.fontsize;
    const char *fontName = config.fontname;
    const float rollTime = config.scrolltime;
    const float holdTime = config.fixtime;
    const float displayArea = config.displayarea;
    const float rollArea = config.scrollarea;
    const int density = config.density;
    const int blockMode = config.blockmode;
    const BOOL saveBlockedPart = config.saveBlockedPart;
    BOOL showMsgBox = config.showMsgBox;
    const BOOL showUserName = config.showUserNames;
    COORDIN msgBoxSize = config.msgBoxSize;
    COORDIN msgBoxPos = config.msgBoxPos;
    int msgFontSize = config.msgboxFontsize;

    /* 刷新status */
    if (status != NULL)
    {
        status -> function = (void *)writeAssDanmakuPart;
        (status -> completedNum) = 0;
        status -> isDone = FALSE;
    }
    
    if(head == NULL || opF == NULL)
    {
        return 1;
    }
    
    if(opF == NULL)
    {
        return 2;
    }
    
    /* 临时变量 */
    int cnt;
    char tempText[MAX_TEXT_LENGTH];

    DANMAKU *now = NULL;
    DANMAKU *signPtr = head, *scanPtr = head;

    MSGLIST *msgListHead = NULL;
    MSGLIST *msgListTail = NULL;
    MSGLIST *msgListPtr;
    
    /* 信息框参数 */
    float msgAnimationTime = 0.5;  /* 新消息弹出动画时间 */
    int msgMarginV = msgFontSize / 6;  /* 消息之前竖直方向间隔 */
    int msgBoxRadius = msgFontSize / 2;  /* 消息框圆角半径 */
    float msgStartTime;  /* 消息开始时间 */

    /* 弹幕占用时间 */
    float msgEndTime = -msgAnimationTime;     /* 上一条消息动画结束时间 */
    float *R2LToRightTime, *R2LToLeftTime;    /* 右左滚动行经过特定点时间 */
    float *L2RToRightTime, *L2RToLeftTime;    /* 左右滚动行经过特定点时间 */
    float *fixEndTime;    /* 顶部与底部弹幕消失时间 */
    
    /* 显示区域限制 */
    int rollResY, holdResY;
    if(displayArea > EPS && displayArea < 1-EPS)
    {/* 大于 0 但小于 1 的情况 */
        /* TODO: 此处可能有逻辑错误 */
        rollResY = holdResY = resolution.y * displayArea;
    }
    else
    {/* 显示区域为 1 (100%) 或 非法输入 */ 
        rollResY = holdResY = resolution.y;
    }
    
    if(fabs(rollArea - 1) > EPS)
    {
        if (resolution.y * rollArea < rollResY)
        {
            rollResY = resolution.y * rollArea;
        }
    }
    
    if((R2LToRightTime = (float *)malloc(rollResY * sizeof(float))) == NULL)
    {
        fclose(opF);
        return 3;
    }
    if((R2LToLeftTime = (float *)malloc(rollResY * sizeof(float))) == NULL)
    {
        free(R2LToRightTime);
        fclose(opF);
        return 4;
    }
    if((L2RToRightTime = (float *)malloc(rollResY * sizeof(float))) == NULL)
    {
        free(R2LToRightTime);
        free(R2LToLeftTime);
        fclose(opF);
        return 5;
    }
    if((L2RToLeftTime = (float *)malloc(rollResY * sizeof(float))) == NULL)
    {
        free(R2LToRightTime);
        free(R2LToLeftTime);
        free(L2RToRightTime);
        fclose(opF);
        return 6;
    }
    if((fixEndTime = (float *)malloc(holdResY * sizeof(float))) == NULL)
    {
        free(R2LToRightTime);
        free(R2LToLeftTime);
        free(L2RToRightTime);
        free(L2RToLeftTime);
        fclose(opF);
        return 7;
    }

    /* 信息框边框与蒙版 */
    char msgBoxClip[MAX_TEXT_LENGTH];
    // sprintf(msgBoxClip, "\\clip(%d,%d,%d,%d)", msgBoxPos.x, msgBoxPos.y, msgBoxPos.x+msgBoxSize.x, msgBoxPos.y+msgBoxSize.y);
    sprintf(msgBoxClip, "\\clip(m %d %d b %d %d %d %d %d %d l %d %d b %d %d %d %d %d %d l %d %d l %d %d)",
        0+msgBoxPos.x, msgBoxRadius+msgBoxPos.y, /* 起点 */
        0+msgBoxPos.x, msgBoxRadius/2+msgBoxPos.y, msgBoxRadius/2+msgBoxPos.x, 
        0+msgBoxPos.y, msgBoxRadius+msgBoxPos.x, 0+msgBoxPos.y, /* 左上圆角 */
        msgBoxSize.x - msgBoxRadius+msgBoxPos.x, 0+msgBoxPos.y, /* 左上直线 */
        msgBoxSize.x - msgBoxRadius/2+msgBoxPos.x, 0+msgBoxPos.y, msgBoxSize.x+msgBoxPos.x, 
        msgBoxRadius/2+msgBoxPos.y, msgBoxSize.x+msgBoxPos.x, msgBoxRadius+msgBoxPos.y, /* 右上圆角 */
        msgBoxSize.x+msgBoxPos.x, msgBoxSize.y+msgBoxPos.y, /* 右边直线 */
        0+msgBoxPos.x, msgBoxSize.y+msgBoxPos.y /* 底线 */
    );

    now = head;
    int listCnt = 0;  /* 序号计数器 */
    while (now != NULL)
    {/* 读链表写ass */
        listCnt++;

        /* 
         * 密度上限屏蔽 及 相同内容弹幕屏蔽
         * 对待特殊弹幕的屏蔽策略：
         * 除非按类型屏蔽，否则不屏蔽特殊弹幕（按密度或重复内容屏蔽无效） 
         * 但特殊弹幕占用弹幕密度，当超过指定密度时，会将普通弹幕挤下来 
         */
        if ((density > 0 || (blockMode & BLK_REPEAT)) && IS_NORMAL(now))
        {
            int danmakuNum = 0;

            while (getEndTime(signPtr, rollTime, holdTime) < now -> time + EPS)
            {/* 移动指针到同屏第一条弹幕 */
                signPtr = signPtr -> next;
            }
            
            scanPtr = signPtr;
            while (scanPtr != now)
            {
                if (scanPtr -> type > 0 && getEndTime(scanPtr, rollTime, holdTime) > now -> time)
                {
                    if (IS_NORMAL(scanPtr) && now -> type > 0 &&
                       (blockMode & BLK_REPEAT) && !strcmp(scanPtr -> text, now -> text))
                    {/* 如果重复内容屏蔽开启且内容重复 */
                        if (now -> type > 0)
                        {
                            now -> type *= -1;
                        }
                        if (saveBlockedPart == FALSE)
                        {
                            goto NEXTNODE;
                        }
                    }
                    else if (IS_NORMAL(scanPtr) || IS_SPECIAL(scanPtr))
                    {/* 如果是一个合法的弹幕 */
                        danmakuNum++;
                    }
                }
                scanPtr = scanPtr -> next;
            }
            
            if (now -> type > 0 && density > 0 && density <= danmakuNum)
            {/* 判断是否达到弹幕密度要求上限并屏蔽 */
                if (now -> type > 0)
                {
                    now -> type *= -1;
                }
                if (saveBlockedPart == FALSE)
                {
                    goto NEXTNODE;
                }
            }
        }
        
        /* 文本长度计算 */
        int textLen = 0, textHei = 0;

        /* 计算用户ID长度 */
        if (showUserName == TRUE && now->user != NULL)
        {
            textLen = getStrLen((unsigned char *)(now->user->name), fontSize, now -> fontSize, fontName);
        }

        /* 计算弹幕内容长度 */
        textLen += getStrLen((unsigned char *)(now -> text), fontSize, now -> fontSize, fontName);
        textHei = getStrHei((unsigned char *)(now -> text), fontSize, now -> fontSize, fontName);

        /* 特殊字符替换 */
        char escapedText[MAX_TEXT_LENGTH];
        assEscape(escapedText, now->text, MAX_TEXT_LENGTH, ASS_ESCAPE);
        // strSafeCopy(escapedText, now->text, MAX_TEXT_LENGTH);
        
        /* 弹幕按类型解析 */
        if(now -> type == 1 || now -> type == -1)/* 右左弹幕 */ 
        {
            int PositionY;
            for(PositionY = 1; PositionY < rollResY - textHei; PositionY++)
            {
                for(cnt = 0; cnt < fontSize; cnt++)
                {
                    if(now->time < R2LToRightTime[PositionY + cnt] || 
                       now->time + rollTime*(float)resolution.x/(resolution.x+textLen) < R2LToLeftTime[PositionY + cnt])
                    {/* 当本条弹幕出现该行最后一条弹幕未离开屏幕右边 或 
                        当本条弹幕到达左端时该行最后一条弹幕没有完全退出屏幕 */
                        PositionY = PositionY + cnt + 1;
                        break;
                    }
                }
                if(cnt >= textHei)
                {
                    break;
                }
            }
            if(PositionY >= rollResY - textHei)
            {
                if(density == -1)
                {
                    now -> type = -1;
                    if (saveBlockedPart == FALSE)
                    {
                        goto NEXTNODE;
                    }
                }
                PositionY = findMin(R2LToRightTime, rollResY, rollResY - textHei, 0);
            }
            
            if (now -> type > 0)
            {
                for(cnt = 0; cnt < textHei; cnt++)
                {/* 登记位置占用信息 */
                    R2LToRightTime[PositionY + cnt] = now -> time + rollTime * (float)textLen / (resolution.x + textLen); 
                    R2LToLeftTime[PositionY + cnt] = now -> time + rollTime;
                }
                fprintf(opF, "\nDialogue: 0,");
            }
            else
            {
                fprintf(opF, "\nComment: 0,");
            }
            
            printTime(opF, now->time, ",");
            printTime(opF, now->time + rollTime, ",");
            fprintf(opF, "R2L,,0000,0000,0000,,{\\move(%d,%d,%d,%d)\\q2",
                    resolution.x + textLen/2, PositionY, -1 * textLen / 2, PositionY);
            
            if(textHei != 25)
            {
                fprintf(opF, "\\fs%d", textHei);
            }
            
            fprintf(opF, "}");
            if (showUserName == TRUE && now->user != NULL)
            {
                fprintf(opF, "{\\c&HBCACF7}%s:\\h", now->user->name);
            }

            if(now -> color != 0xFFFFFF || showUserName == TRUE)
            {
                char hexColor[7];
                fprintf(opF, "{\\c&H%s}", toHexColor(now->color, hexColor));
            }

            fprintf(opF, "%s", escapedText);
        }
        else if(now -> type == 2 || now -> type == -2)/* 左右弹幕 */ 
        {
            int PositionY;
            for(PositionY = 1; PositionY < rollResY - textHei; PositionY++)
            {
                for(cnt = 0; cnt < textHei; cnt++)
                {
                    if(now->time < L2RToRightTime[PositionY + cnt] || 
                       now->time + rollTime*(float)resolution.x/(resolution.x+textLen) < L2RToLeftTime[PositionY + cnt])
                    {/* 当本条弹幕出现该行最后一条弹幕未离开屏幕左边 或 
                        当本条弹幕到达右端时该行最后一条弹幕没有完全退出屏幕 */
                        PositionY = PositionY + cnt + 1;
                        break;
                    }
                }
                if(cnt >= textHei)
                {
                    break;
                }
            }
            if(PositionY >= rollResY - textHei)
            {
                if(density == -1)
                {
                    now -> type = -2;
                    if (saveBlockedPart == FALSE)
                    {
                        goto NEXTNODE;
                    }
                }
                PositionY = findMin(L2RToRightTime, rollResY, rollResY - textHei, 0);
            }
            
            if (now -> type > 0)
            {
                for(cnt = 0; cnt < textHei; cnt++)
                {/* 登记位置占用信息 */
                    L2RToRightTime[PositionY + cnt] = now -> time + rollTime * (float)textLen / (resolution.x + textLen); 
                    L2RToLeftTime[PositionY + cnt] = now -> time + rollTime;
                }
                
                fprintf(opF, "\nDialogue: 0,");
            }
            else
            {
                fprintf(opF, "\nComment: 0,");
            }
            
            printTime(opF, now->time, ",");
            printTime(opF, now->time + rollTime, ",");
            fprintf(opF, "L2R,,0000,0000,0000,,{\\move(%d,%d,%d,%d)\\q2",
                    -1 * textLen / 2, PositionY, resolution.x + textLen/2, PositionY);
            
            if(textHei != 25)
            {
                fprintf(opF, "\\fs%d", textHei);
            }
            
            fprintf(opF, "}");
            if (showUserName == TRUE && now->user != NULL)
            {
                fprintf(opF, "{\\c&HBCACF7}%s:", now->user->name);
            }

            if(now -> color != 0xFFFFFF || showUserName == TRUE)
            {
                char hexColor[7];
                fprintf(opF, "{\\c&H%s}", toHexColor(now->color, hexColor));
            }

            fprintf(opF, "%s", escapedText);
        }
        else if(now -> type == 3 || now -> type == -3)/* 顶端弹幕 */ 
        {
            int PositionY;
            for(PositionY = 1; PositionY < holdResY - textHei; PositionY++)
            {
                for(cnt = 0; cnt < textHei; cnt++)
                {
                    if(now->time < fixEndTime[PositionY + cnt])
                    {/* 当本条弹幕出现时本行上一条弹幕还没有消失 */
                        PositionY = PositionY + cnt + 1;
                        break;
                    }
                }
                if(cnt >= textHei)
                {
                    break;
                }
            }
            if(PositionY >= holdResY - textHei)
            {
                if(density == -1)
                {
                    now -> type = -3;
                    if (saveBlockedPart == FALSE)
                    {
                        goto NEXTNODE;
                    }
                }
                PositionY = findMin(fixEndTime, holdResY, holdResY - textHei, 0);
            }
            
            if (now -> type > 0)
            {
                for(cnt = 0; cnt < textHei; cnt++)
                {/* 登记占用信息 */ 
                    fixEndTime[PositionY + cnt] = now -> time + holdTime;
                }
                fprintf(opF, "\nDialogue: 0,");
            }
            else
            {
                fprintf(opF, "\nComment: 0,");
            }
            
            printTime(opF, now->time, ",");
            printTime(opF, now->time + holdTime, ",");
            fprintf(opF, "TOP,,0000,0000,0000,,{\\pos(%d,%d)\\q2", resolution.x / 2, PositionY);
            
            if(textHei != 25)
            {
                fprintf(opF, "\\fs%d", textHei);
            }
            
            fprintf(opF, "}");
            if (showUserName == TRUE && now->user != NULL)
            {
                fprintf(opF, "{\\c&HBCACF7}%s:", now->user->name);
            }

            if(now -> color != 0xFFFFFF || showUserName == TRUE)
            {
                char hexColor[7];
                fprintf(opF, "{\\c&H%s}", toHexColor(now->color, hexColor));
            }

            fprintf(opF, "%s", escapedText);
        }
        else if(now -> type == 4 || now -> type == -4)/* 底端弹幕 */ 
        {
            int PositionY;
            for(PositionY = holdResY - 1; PositionY > textHei - 1; PositionY--)
            {
                for(cnt = 0; cnt < textHei; cnt++)
                {
                    if(now->time < fixEndTime[PositionY - cnt])
                    {/* 当本条弹幕出现时本行上一条弹幕还没有消失 */
                        PositionY = PositionY - cnt - 1;
                        break;
                    }
                }
                if(cnt >= textHei)
                {
                    break;
                }
            }
            if(PositionY < textHei)
            {
                if(density == -1)
                {
                    now -> type = -4;
                    if (saveBlockedPart == FALSE)
                    {
                        goto NEXTNODE;
                    }
                }
                PositionY = findMin(fixEndTime, holdResY, textHei, 1);
            }
            for(cnt = 0; cnt < textHei; cnt++)
            {
                fixEndTime[PositionY - cnt] = now -> time + holdTime;
            }
            
            if (now -> type > 0)
            {
                for(cnt = 0; cnt < textHei; cnt++)
                {/* 登记占用信息 */ 
                    fixEndTime[PositionY - cnt] = now -> time + holdTime;
                }
                fprintf(opF, "\nDialogue: 0,");
            }
            else
            {
                fprintf(opF, "\nComment: 0,");
            }
            
            printTime(opF, now->time, ",");
            printTime(opF, now->time + holdTime, ",");
            fprintf(opF, "BTM,,0000,0000,0000,,{\\pos(%d,%d)\\q2",
                    resolution.x / 2, PositionY - textHei + 2);
            
            if(textHei != 25)
            {
                fprintf(opF, "\\fs%d", textHei);
            }
            
            fprintf(opF, "}");
            if (showUserName == TRUE && now->user != NULL)
            {
                fprintf(opF, "{\\c&HBCACF7}%s:", now->user->name);
            }

            if(now -> color != 0xFFFFFF || showUserName == TRUE)
            {
                char hexColor[7];
                fprintf(opF, "{\\c&H%s}", toHexColor(now->color, hexColor));
            }

            fprintf(opF, "%s", escapedText);
        }
        else if(now -> type == 5 || now -> type == -5)/* 特殊弹幕 */
        {
            if (saveBlockedPart == FALSE)
            {
                goto NEXTNODE;
            }
            
            float n7ExistTime;
            int n7MoveTime, n7PauseTime;
            float n7StartX, n7StartY, n7EndX, n7EndY;
            int n7FadeStart, n7FadeEnd, n7FrY, n7FrZ;
            
            n7StartX = now -> special -> startX;
            n7StartY = now -> special -> startY;
            n7FadeStart = now -> special -> fadeStart;    
            n7FadeEnd = now -> special -> fadeEnd;    
            n7ExistTime = now -> special -> existTime;
            n7FrZ = now -> special -> frZ;
            n7FrY = now -> special -> frY;
            n7EndX = now -> special -> endX;
            n7EndY = now -> special -> endY;
            n7MoveTime = now -> special -> moveTime;
            n7PauseTime = now -> special -> pauseTime;
            
            /* 写到字幕文件 */
            if (now -> type < 0)
            {
                fprintf(opF, "\nComment: 0,");
            }
            else
            {
                fprintf(opF, "\nDialogue: 0,");
            }
            
            printTime(opF, now->time, ",");
            printTime(opF, now->time + n7ExistTime, ",");
            fprintf(opF, "SP,,0000,0000,0000,,{");
            if( (n7StartX < 1+EPS) && (n7EndX < 1+EPS) && (n7StartY < 1+EPS) && (n7EndY < 1+EPS) )
            {
                n7StartX *= resolution.x;
                n7EndX *= resolution.x;
                n7StartY *= resolution.y;
                n7EndY *= resolution.y;
            }
            if(n7StartX == n7EndX && n7StartY == n7EndY)
            {/* 固定位置 */ 
                fprintf(opF, "\\pos(%d,%d)", (int)n7StartX, (int)n7StartY);
            }
            else
            {/* 移动位置 */ 
                fprintf(opF, "\\move(%d,%d,%d,%d", (int)n7StartX, (int)n7StartY, (int)n7EndX, (int)n7EndY);
                if(n7PauseTime == 0)
                {
                    if(n7MoveTime == 0)
                    {
                        fprintf(opF, ")");
                    }
                    else
                    {
                        fprintf(opF, ",0,%d)", n7MoveTime);
                    }
                }
                else
                {
                    fprintf(opF, ",%d,%d)", n7PauseTime, n7MoveTime + n7PauseTime);
                }
            }
            fprintf(opF, "\\q2");
            
            if(now -> fontSize != 25)
            {/* 字号 */ 
                fprintf(opF, "\\fs%d", now -> fontSize);
            }
            
            if(now -> color != 0xFFFFFF)
            {/* 颜色 */ 
                char hexColor[7];
                fprintf(opF, "\\c&H%s", toHexColor(now->color, hexColor));
            }
            
            if(n7FrY != 0)
            {/* Y轴旋转 */ 
                fprintf(opF, "\\fry%d", n7FrY);
            }
            if(n7FrZ != 0)
            {
                fprintf(opF, "\\frz%d", n7FrZ);
            }
            
            if(n7FadeStart != 0 || n7FadeEnd != 0)/* 0-255 越大越透明 */
            {/* fade(淡入透明度,实体透明度,淡出透明度,淡入开始时间,淡入结束时间,淡出开始时间,淡出结束时间) */
                fprintf(opF, "\\fade(%d,%d,%d,0,0,%d,%d)", n7FadeStart, n7FadeStart,
                        n7FadeEnd, n7PauseTime, (int)(n7ExistTime * 1000));
            }
            else
            {
                fprintf(opF, "\\alpha&H00");
            }
            
            if(strlen(now -> special -> fontName) > 0)
            {/* 字体 */ 
                fprintf(opF, "\\fn%s", now -> special -> fontName);
            }

            fprintf(opF, "}%s", escapedText);
        }
        else if (now -> type == 8)/* 代码弹幕 */ 
        {
            fprintf(opF, "\nComment: NO.%d(Code danmaku):Unable to read this type.", listCnt);
        }
        else if (IS_MSG(now) && showMsgBox)/* 消息 */
        {
            /* 按最低价格屏蔽 */
            if (now->gift->price < config.giftMinPrice && now->gift->price > -EPS)
            {
                goto NEXTNODE;
            }

            int totalHeight = 0;
            MSGLIST *newMsgNode;

            if((newMsgNode = (MSGLIST *)malloc(sizeof(MSGLIST))) == NULL)
            {
                /* TODO: 异常处理 */
                fclose(opF);
                return 3;
            }

            if (msgListTail != NULL)
            {
                msgStartTime = msgListHead->message -> time;
            }
            else
            {
                msgStartTime = 0.0;
            }

            /* 填入数据 */
            newMsgNode -> isShown = FALSE;
            newMsgNode -> message = now;
            newMsgNode -> height = getMsgBoxHeight(newMsgNode->message, msgFontSize, msgBoxSize.x) + msgMarginV;

            /* 链表连接 */
            if (msgListTail == NULL)
            {/* 成为首个元素 */
                msgListTail = msgListHead = newMsgNode;
                newMsgNode->nextNode = NULL;
                newMsgNode->lastNode = NULL;
            }
            else
            {/* 并入队头 */                
                newMsgNode->lastNode = NULL;
                msgListHead->lastNode = newMsgNode;
                newMsgNode->nextNode = msgListHead;
                msgListHead = newMsgNode;
            }

            if (now->time - msgAnimationTime < msgStartTime)
            {
                goto NEXTNODE;
            }
            
            /* 刷新显示 */
            msgListPtr = msgListHead -> nextNode;
            while (msgListPtr != NULL && msgListPtr->isShown == FALSE)
            {/* 计算需显示消息总高度 */
                totalHeight += msgListPtr->height;
                msgListPtr = msgListPtr -> nextNode;
            }

            msgListPtr = msgListTail;
            while (msgListPtr != NULL && msgListPtr->isShown == TRUE)
            {/* 上次在场的消息常驻显示 */
                printMessage(opF, msgBoxPos.x, msgListPtr->posY, msgBoxPos.x, msgListPtr->posY, msgEndTime, msgStartTime, 
                             msgBoxSize.x, msgFontSize, msgBoxClip, msgListPtr->message);
                msgListPtr = msgListPtr -> lastNode;
            }
            
            msgListPtr = msgListTail;
            while (msgListPtr != NULL && msgListPtr->isShown == TRUE)
            {/* 旧消息向上滚动 */
                printMessage(opF, msgBoxPos.x, msgListPtr->posY, msgBoxPos.x, msgListPtr->posY-totalHeight,
                             msgStartTime, msgStartTime + msgAnimationTime/2.0, 
                             msgBoxSize.x, msgFontSize, msgBoxClip, msgListPtr->message);
                msgListPtr->posY -= totalHeight;
                
                if (msgListPtr->posY < msgBoxPos.y - msgListPtr->height)
                {
                    MSGLIST *msgListNextPtr;
                    msgListTail = msgListNextPtr = msgListPtr -> lastNode;
                    if (msgListPtr -> lastNode != NULL)
                    {
                        msgListPtr->lastNode -> nextNode = msgListPtr -> nextNode;
                    }
                    if (msgListPtr -> nextNode != NULL)
                    {
                        msgListPtr->nextNode -> lastNode = msgListPtr -> lastNode;
                    }
                    free(msgListPtr);

                    msgListPtr = msgListNextPtr;
                }
                else
                {
                    printMessage(opF, msgBoxPos.x, msgListPtr->posY, msgBoxPos.x, msgListPtr->posY,
                                 msgStartTime + msgAnimationTime/2.0, msgStartTime + msgAnimationTime, 
                                 msgBoxSize.x, msgFontSize, msgBoxClip, msgListPtr->message);
                    msgListPtr = msgListPtr -> lastNode;
                }
            }
            
            msgListPtr = msgListHead -> nextNode;
            totalHeight = msgBoxPos.y + msgBoxSize.y;
            while (msgListPtr != NULL && msgListPtr->isShown == FALSE)
            {/* 新消息进场 */
                msgListPtr->isShown = TRUE;
                totalHeight -= msgListPtr -> height;
                msgListPtr -> posY = totalHeight;
                
                printMessage(opF, msgBoxPos.x-100, msgListPtr->posY, msgBoxPos.x, msgListPtr->posY,
                                 msgStartTime + msgAnimationTime/2.0, msgStartTime + msgAnimationTime, 
                                 msgBoxSize.x, msgFontSize, "", msgListPtr->message);  
                msgListPtr = msgListPtr -> nextNode;
            }

            msgEndTime = msgStartTime + msgAnimationTime;
        }
        else if (now -> type > 0)/* 类型错误 */ 
        {
            fprintf(opF, "\nComment: NO.%d:unknow type", listCnt);
        }
        
        if (ferror(opF))
        {
            fclose(opF);
            return 9;
        }
        
        NEXTNODE:
        now = now -> next;
        
        /* 刷新status */
        if (status != NULL)
        {
            (status -> completedNum)++;
        }
    }

    /* 剩余消息保持显示 */
    if (msgListTail != NULL && showMsgBox)
    {
        int totalHeight = 0;
        msgStartTime = msgListHead->message->time;

        msgListPtr = msgListHead;
        while (msgListPtr != NULL && msgListPtr->isShown == FALSE)
        {/* 计算需显示消息总高度 */
            totalHeight += msgListPtr->height;
            msgListPtr = msgListPtr -> nextNode;
        }

        msgListPtr = msgListTail;
        while (msgListPtr != NULL && msgListPtr->isShown == TRUE)
        {/* 上次在场的消息常驻显示 */
            printMessage(opF, msgBoxPos.x, msgListPtr->posY, msgBoxPos.x, msgListPtr->posY, msgEndTime, msgStartTime, 
                         msgBoxSize.x, msgFontSize, msgBoxClip, msgListPtr->message);
            msgListPtr = msgListPtr -> lastNode;
        }
        
        msgListPtr = msgListTail;
        while (msgListPtr != NULL && msgListPtr->isShown == TRUE)
        {/* 旧消息向上滚动 */
            printMessage(opF, msgBoxPos.x, msgListPtr->posY, msgBoxPos.x, msgListPtr->posY-totalHeight,
                            msgStartTime, msgStartTime + msgAnimationTime/2.0, 
                            msgBoxSize.x, msgFontSize, msgBoxClip, msgListPtr->message);
            msgListPtr->posY -= totalHeight;
            
            if (msgListPtr->posY >= msgBoxPos.y - msgListPtr->height)
            {
                printMessage(opF, msgBoxPos.x, msgListPtr->posY, msgBoxPos.x, msgListPtr->posY,
                                msgStartTime + msgAnimationTime/2.0, msgStartTime + msgAnimationTime, 
                                msgBoxSize.x, msgFontSize, msgBoxClip, msgListPtr->message);
            }

            msgListPtr = msgListPtr -> lastNode;
        }
        
        msgListPtr = msgListHead;
        totalHeight = msgBoxPos.y + msgBoxSize.y;
        while (msgListPtr != NULL && msgListPtr->isShown == FALSE)
        { /* 新消息进场 */
            msgListPtr->isShown = TRUE;
            totalHeight -= msgListPtr -> height;
            msgListPtr -> posY = totalHeight;
            printMessage(opF, msgBoxPos.x-100, msgListPtr->posY, msgBoxPos.x, msgListPtr->posY,
                                msgStartTime + msgAnimationTime/2.0, msgStartTime + msgAnimationTime, 
                                msgBoxSize.x, msgFontSize, "", msgListPtr->message);   
            msgListPtr = msgListPtr -> nextNode;
        }

        msgListPtr = msgListTail;
        while (msgListPtr != NULL && msgListPtr->isShown == TRUE)
        { /* 最后一屏常驻显示 */
            printMessage(opF, msgBoxPos.x, msgListPtr->posY, msgBoxPos.x, msgListPtr->posY,
                        msgStartTime + msgAnimationTime, msgStartTime + msgAnimationTime + 30, 
                        msgBoxSize.x, msgFontSize, msgBoxClip, msgListPtr->message);
            msgListPtr = msgListPtr -> lastNode;
        }
    }
    
    /* 归还空间 */
    free(R2LToRightTime);
    free(R2LToLeftTime);
    free(L2RToRightTime);
    free(L2RToLeftTime);
    free(fixEndTime);

    msgListPtr = msgListHead;
    while (msgListPtr != NULL)
    {
        MSGLIST *thisNode = msgListPtr;
        msgListPtr = msgListPtr->nextNode;
        free(thisNode);
    }
    
    /* 清空缓冲区 */
    fflush(opF);
    
    /* 刷新status */
    if (status != NULL)
    {
        status -> isDone = TRUE;
    }
    return 0;
}

/* 
 * 写ass字幕的统计部分 
 * 参数：
 * 输出文件名/链表头/模式（位运算 TABLE 数据表 / HISTOGRAM 直方图）/
 * 分辨率宽/分辨率高/滚动速度（滚动弹幕）/停留时间（现隐弹幕）/
 * 字号/字体(utf-8)/屏蔽（右左/左右/顶部/底部/特殊/彩色/重复） 
 * 返回值：
 * 0 正常退出
 * 1 链表为空
 * 2 追加文件失败 
  */ 
int writeAssStatPart(FILE *opF, DANMAKU *head, int mode, const int rollTime, const int holdTime,
                      const int density, const int blockMode)
{
    if(mode == 0)
    {
        return 0;
    }
    
    if(head == NULL)
    {
        return 1;
    }
    
    DANMAKU *now = NULL;
    int cnt;
    
    /* 下列数组下标表示弹幕类型，其中下标0表示总数，其值记录了弹幕的条数，做成数组便于利用循环 */ 
    int screen[NUM_OF_TYPE + 1] = {0};/* 同屏弹幕 */ 
    int block[NUM_OF_TYPE + 1] = {0};/* 被屏蔽的弹幕 */ 
    int count[NUM_OF_TYPE + 1] = {0};/* 未屏蔽弹幕计数器 */ 
    int total[NUM_OF_TYPE + 1] = {0};/* 总弹幕数量 */
    
    int pointShiftY = 0;/* Y轴相对标准位置向下偏移的高度 */
    float startTime, endTime;/* stat本条记录的开始与结束时间 */
    float lastDanmakuStartTime = 0.00, lastDanmakuEndTime = 0.00;/* 最后一条弹幕的出场与消失的时间 */
    
    DANMAKU *signPtr = NULL, *scanPtr = NULL;
    
    if(!(mode & TABLE))
    {/* 如果不需要数据框，直方图就要上移 */ 
        pointShiftY = -255; 
    }
    
    /* 画弹幕直方图 */
    if(mode & HISTOGRAM)
    {
        int mPointX;/* 画图时x坐标的变量 */
        
        int chartMaxHeight = 0;/* 图表最高点的高度 */ 
        int allCnt[212] = {0}, blockCnt[212] = {0};
         
        BOOL drawBlockChart = FALSE;
        
        /* 计算各类弹幕总数 */
        now = head;
        while(now != NULL)
        {
            if(abs(now -> type) <= 5)
            {
                total[abs(now -> type)]++;
            }
            
            if(now -> next == NULL)
            {
                lastDanmakuStartTime = now -> time;
                lastDanmakuEndTime = getEndTime(now, rollTime, holdTime);
            }
            now = now -> next;
        }
        /* 使用0位置存储总数 */
        total[0] = total[1] + total[2] + total[3] + total[4] + total[5];
        
        endTime = lastDanmakuStartTime + 30;
        /* 根据偏移量选择正确的分布图框 */
        if(pointShiftY == 0)
        {
            fprintf(opF, "\nDialogue: 5,0:00:00.00,");
            printTime(opF, endTime, ",");
            /* 底框 */ 
            fprintf(opF, "danmakuFactory_stat,,0000,0000,0000,,{\\an7\\1c&HEAF3CE\\p1}m 20 290"
                         " b 20 284 29 275 35 275 l 430 275 b 436 275 445 284 445 290 l 445 370"
                         " b 445 376 436 385 430 385 l 35 385 b 29 385 20 376 20 370 l 20 290{\\p0}");
            
            fprintf(opF, "\nDialogue: 6,0:00:00.00,");
            printTime(opF, lastDanmakuEndTime, ",");
            /* 移动的进度条 */
            fprintf(opF, "danmakuFactory_stat,,0000,0000,0000,,{\\an7\\move(20, 275, 444, 275)\\clip(m 20 290 b 20 284 29"
                         " 275 35 275 l 430 275 b 436 275 445 284 445 290 l 445 370 b 445 376 436 385 430 385"
                         " l 35 385 b 29 385 20 376 20 370 l 20 290)\\1c&HCECEF3\\p1}"
                         "m 0 0 l -425 0 l -425 110 l 0 110 l 0 0{\\p0}");
            fprintf(opF, "\nDialogue: 6,");
            printTime(opF, lastDanmakuEndTime, ",");
            printTime(opF, endTime, ",");
            /* 进度条满之后的样子 */
            fprintf(opF, "danmakuFactory_stat,,0000,0000,0000,,{\\an7\\pos(445, 275)\\clip(m 20 290 b 20 284 29 275 35 275"
                         " l 430 275 b 436 275 445 284 445 290 l 445 370 b 445 376 436 385 430 385 l 35 385"
                         " b 29 385 20 376 20 370 l 20 290)\\1c&HCECEF3\\p1}"
                         "m 0 0 l -425 0 l -425 110 l 0 110 l 0 0{\\p0}");
        }
        else if(pointShiftY == -255)
        {
            fprintf(opF, "\nDialogue: 5,0:00:00.00,");
            printTime(opF, endTime, ",");
            /* 底框 */ 
            fprintf(opF, "danmakuFactory_stat,,0000,0000,0000,,{\\an7\\1c&HEAF3CE\\p1}m 20 35"
                         " b 20 29 29 20 35 20 l 430 20 b 436 20 445 29 445 35 l 445 115"
                         " b 445 121 436 130 430 130 l 35 130 b 29 130 20 121 20 115 l 20 35{\\p0}");
            
            fprintf(opF, "\nDialogue: 6,0:00:00.00,");
            printTime(opF, lastDanmakuEndTime, ",");
            /* 移动的进度条 */
            fprintf(opF, "danmakuFactory_stat,,0000,0000,0000,,{\\an7\\move(20, 20, 444, 20)\\clip(m 20 35 b 20 29 29 20"
                         " 35 20 l 430 20 b 436 20 445 29 445 35 l 445 115 b 445 121 436 130 430 130 l 35 130"
                         " b 29 130 20 121 20 115 l 20 35)\\1c&HCECEF3\\p1}"
                         "m 0 0 l -425 0 l -425 110 l 0 110 l 0 0{\\p0}");
            fprintf(opF, "\nDialogue:6,");
            printTime(opF, lastDanmakuEndTime, ",");
            printTime(opF, endTime, ",");
            /* 进度条满之后的样子 */
            fprintf(opF, "danmakuFactory_stat,,0000,0000,0000,,{\\an7\\pos(445, 20)\\clip(m 20 35 b 20 29 29 20"
                         " 35 20 l 430 20 b 436 20 445 29 445 35 l 445 115 b 445 121 436 130 430 130 l 35 130"
                         " b 29 130 20 121 20 115 l 20 35)\\1c&HCECEF3\\p1}"
                         "m 0 0 l -425 0 l -425 110 l 0 110 l 0 0{\\p0}");
        }
        
        now = head;
        while(now != NULL)
        {/* 统计各时间段弹幕数量 */
            for(cnt = (int)(now->time / lastDanmakuEndTime * 211); 
                cnt < (int)(getEndTime(now, rollTime, holdTime) / lastDanmakuEndTime * 211); cnt++)
            {/* 从开始时间写到结束时间 */
                allCnt[cnt]++;
                if(now -> type < 0)
                {
                    blockCnt[cnt]++;
                    drawBlockChart = TRUE;
                }
            }
            now = now -> next;
        }
        
        for(cnt = 0; cnt < 212; cnt++)
        {/* 寻找最大高度 */ 
            if(allCnt[cnt] > chartMaxHeight)
            {
                chartMaxHeight = allCnt[cnt];
            }
        }
        
        /* 画全部弹幕分布图 */
        fprintf(opF, "\nDialogue:7,0:00:00.00,");
        printTime(opF, endTime, ",");
        if(pointShiftY == 0)
        {/* 分布图起始段 */ 
            fprintf(opF, "danmakuFactory_stat,,0000,0000,0000,,{\\an7\\clip(m 20 290 b 20 284 29 275 35 275 l 430 275"
                         " b 436 275 445 284 445 290 l 445 370 b 445 376 436 385 430 385 l 35 385"
                         " b 29 385 20 376 20 370 l 20 290)\\1c&HFFFFFF\\p1}m 20 385");
        }
        else if(pointShiftY == -255)
        {
            fprintf(opF, "danmakuFactory_stat,,0000,0000,0000,,{\\an7\\clip(m 20 35 b 20 29 29 20"
                         " 35 20 l 430 20 b 436 20 445 29 445 35 l 445 115 b 445 121 436 130 430 130 l 35 130"
                         " b 29 130 20 121 20 115 l 20 35)\\1c&HFFFFFF\\p1}m 20 130");
        }
        
        mPointX = 21;
        for(cnt = 0; cnt < 212; cnt++)
        {/* 根据统计数据画图，最高高度是110px */ 
            if(cnt == 1)
            {/* 第一根线 */ 
                fprintf(opF, " l %d %d",
                        mPointX, 385 - (int)((float)allCnt[cnt] / chartMaxHeight * 110) + pointShiftY);
                mPointX += 2;
            }
            
            /* 横向画线让柱子有一定宽度 */ 
            fprintf(opF, " l %d %d",
                        mPointX, 385 - (int)((float)allCnt[cnt] / chartMaxHeight * 110) + pointShiftY);
            
            
            if(cnt == 211)
            {/* 最后一条线连接到图形右下角 */ 
                fprintf(opF, " l %d %d", mPointX, 385 + pointShiftY);
            }
            else
            {/* 纵向画线，将线条画到下一个数据点高度 */ 
                fprintf(opF, " l %d %d",
                        mPointX, 385 - (int)((float)allCnt[cnt + 1] / chartMaxHeight * 110) + pointShiftY);
                mPointX += 2;
            }
        }
        
        /* 封闭图形 */
        fprintf(opF, " l 20 385{\\p0}");
        
        /* 画屏蔽弹幕分布图 */ 
        if(drawBlockChart == TRUE)
        {
            mPointX = 21;
            fprintf(opF, "\nDialogue: 8,0:00:00.00,");
            printTime(opF, endTime, ",");
            
            if(pointShiftY == 0)
            {
                fprintf(opF, "danmakuFactory_stat,,0000,0000,0000,,{\\an7\\clip(m 20 290 b 20 284 29 275 35 275 l 430 275"
                             " b 436 275 445 284 445 290 l 445 370 b 445 376 436 385 430 385 l 35 385"
                             " b 29 385 20 376 20 370 l 20 290)\\1c&HD3D3D3\\p1}m 20 385");
            }
            else if(pointShiftY == -255)
            {
                fprintf(opF, "danmakuFactory_stat,,0000,0000,0000,,{\\an7\\clip(m 20 35 b 20 29 29 20"
                             " 35 20 l 430 20 b 436 20 445 29 445 35 l 445 115 b 445 121 436 130 430 130 l 35 130"
                             " b 29 130 20 121 20 115 l 20 35)\\1c&HD3D3D3\\p1}m 20 130");
            }
            for(cnt = 0; cnt < 212; cnt++)
            {/* 根据统计数据画图，最高高度是110px */ 
                if(cnt == 1)
                {/* 第一根线 */ 
                    fprintf(opF, " l %d %d",
                            mPointX, 385 - (int)((float)blockCnt[cnt] / chartMaxHeight * 110) + pointShiftY);
                    mPointX += 2;
                }
                
                /* 横向画线让柱子有一定宽度 */
                fprintf(opF, " l %d %d",
                            mPointX, 385 - (int)((float)blockCnt[cnt] / chartMaxHeight * 110) + pointShiftY);
                
                if(cnt == 211)
                {/* 最后一条线连接到图形右下角 */
                    fprintf(opF, " l %d %d", mPointX, 385 + pointShiftY);
                }
                else
                {/* 纵向画线，将线条画到下一个数据点高度 */ 
                    fprintf(opF, " l %d %d",
                            mPointX, 385 - (int)((float)blockCnt[cnt + 1] / chartMaxHeight * 110) + pointShiftY);
                    mPointX += 2;
                }
            }
            /* 封闭图形 */
            fprintf(opF, " l 20 385{\\p0}");
        }
    }

    
    /* 显示弹幕统计表 */
    if(mode & TABLE)
    {
        /* 非法数据拦截 部分类型未做统计模式 */
        now = head;
        while(now != NULL)
        {
            if (!IS_NORMAL(now) && !IS_SPECIAL(now))
            {
                return 0;
            }
            now = now->next;
        }

        now = head;
        if(!total[0])
        {/* 统计出每种弹幕的总数 如果之前没有计算过则计算一遍 */
            while(now != NULL)
            {
                if (abs(now -> type) <= NUM_OF_TYPE)
                {
                    total[abs(now -> type)]++;
                }
                
                if (now -> next == NULL)
                {
                    lastDanmakuStartTime = now -> time;
                }
                now = now -> next;
            }
            /* 使用0位置存储总数 */
            total[0] = total[1] + total[2] + total[3] + total[4] + total[5];
        }
        
        /* 画表格形状及上面的常驻或初始数据 */
        endTime = lastDanmakuStartTime + 30;
        fprintf(opF, "\n\nDialogue:3,0:00:00.00,");
        printTime(opF, endTime, ",");
        fprintf(opF, "danmakuFactory_stat,,0000,0000,0000,,{\\an7\\1c&HCECEF3\\p1}"
                     "m 20 35 b 20 26 26 20 35 20 l 430 20 b 439 20 445 29 445 35 l 445 50 l 20 50 l 20 35 "
                     "m 20 80 l 445 80 l 445 110 l 20 110 l 20 110 l 20 80 m 20 140 l 445 140 l 445 170 l 20"
                     " 170 l 20 170 l 20 140 m 20 200 l 445 200 l 445 230 l 20 230 l 20 230 l 20 200{\\p0}");
        fprintf(opF, "\nDialogue:4,0:00:00.00,");
        printTime(opF, endTime, ",");
        fprintf(opF, "danmakuFactory_stat,,0000,0000,0000,,{\\an7\\1c&HEAF3CE\\p1}"
                     "m 20 50 l 445 50 l 445 80 l 20 80 l 20 80 l 20 50 m 20 110 l 445 110 l 445 140 l 20 140"
                     " l 20 140 l 20 110 m 20 170 l 445 170 l 445 200 l 20 200 l 20 200 l 20 170 m 20 230"
                     " l 445 230 l 445 230 l 445 245 b 445 251 436 260 430 260 l 35 260 b 29 260 20 251 20 245"
                     " l 20 230{\\p0}");
        
        /* 显示表格文字 注*调试模式的文字统一在第十层显示 */
        printStatDataStr(opF, 0.00, endTime,  62, 35, NULL, "type");
        printStatDataStr(opF, 0.00, endTime, 147, 35, NULL, "screen");
        printStatDataStr(opF, 0.00, endTime, 232, 35, NULL, "blockCnt");
        printStatDataStr(opF, 0.00, endTime, 317, 35, NULL, "count");
        printStatDataStr(opF, 0.00, endTime, 402, 35, NULL, "total");

        fprintf(opF, "\nDialogue:10,0:00:00.00,");
        printTime(opF, endTime, ",");
        fprintf(opF, "danmakuFactory_stat,,0000,0000,0000,,{\\an4\\pos(35,245)\\b1\\fs25\\1c&HFFFFFF"
                     "}DanmakuFactory by hkm");
        
        /* 显示弹幕类型表头，如果被屏蔽就使用划线 */
        if(  !(blockMode & BLK_R2L)  || !(blockMode & BLK_L2R)    || 
             !(blockMode & BLK_TOP)  || !(blockMode & BLK_BOTTOM) || 
             !(blockMode & BLK_SPECIAL)
          )
        {
            printStatDataStr(opF, 0.00, endTime, 62, 65, NULL, "ALL");
        }
        else
        {
            printStatDataStr(opF, 0.00, endTime, 62, 65, "\\s1", "ALL");
        }
        if(!(blockMode & BLK_R2L))
        {
            printStatDataStr(opF, 0.00, endTime, 62, 95, NULL, "R to L");
        }
        else
        {
            printStatDataStr(opF, 0.00, endTime, 62, 95, "\\s1", "R to L");
        }
        if(!(blockMode & BLK_L2R))
        {
            printStatDataStr(opF, 0.00, endTime, 62, 125, NULL, "L to R");
        }
        else
        {
            printStatDataStr(opF, 0.00, endTime, 62, 125, "\\s1", "L to R");
        }
        if(!(blockMode & BLK_TOP))
        {
            printStatDataStr(opF, 0.00, endTime, 62, 155, NULL, "TOP");
        }
        else
        {
            printStatDataStr(opF, 0.00, endTime, 62, 155, "\\s1", "TOP");
        }
        if(!(blockMode & BLK_BOTTOM))
        {
            printStatDataStr(opF, 0.00, endTime, 62, 185, NULL, "BOTTOM");
        }
        else
        {
            printStatDataStr(opF, 0.00, endTime, 62, 185, "\\s1", "BOTTOM");
        }
        if(!(blockMode & BLK_SPECIAL))
        {
            printStatDataStr(opF, 0.00, endTime, 62, 215, NULL, "SPCIAL");
        }
        else
        {
            printStatDataStr(opF, 0.00, endTime, 62, 215, "\\s1", "SPCIAL");
        }
        
        /* 统一计算显示total的值 */
        for(cnt = 0; cnt < 6; cnt++)
        {
            printStatDataInt(opF, 0.00, endTime, 402, 65 + cnt * 30, NULL, total[cnt]);
            if(total[cnt] == 0)
            {/* 总数是0就开始统一显示0 */
                printStatDataInt(opF, 0.00, endTime, 147, 65 + cnt * 30, NULL, 0);
                printStatDataInt(opF, 0.00, endTime, 317, 65 + cnt * 30, NULL, 0);
                printStatDataInt(opF, 0.00, endTime, 232, 65 + cnt * 30, NULL, 0);
            }
        }
        
        if(head -> time > EPS)
        {/* 如果第一条弹幕不是从0.00开始就将全部数据写0 即head -> time > 0 */
            int i, j;
            for(i = 0; i < 3; i++)
            {
                for(j = 0; j <= NUM_OF_TYPE; j++)
                {
                    printStatDataInt(opF, 0.00, head -> time, 147 + i * 85, 65 + j * 30, NULL, 0);
                }
            }
        }
        fprintf(opF, "\n");
        
        /* 遍历整个链表打印全部变化的数据 */
        signPtr = now = head;
        while(now != NULL)
        {
            /* 移动指针到同屏第一条弹幕 */
            while(getEndTime(signPtr, rollTime, holdTime) < now -> time + EPS)
            {
                signPtr = signPtr -> next;
            }
            
            /* 更新同屏弹幕计数器 */ 
            scanPtr = signPtr;
            arrset(screen, 0, NUM_OF_TYPE + 1);
            while(scanPtr != now -> next)
            {
                if(scanPtr -> type > 0 && getEndTime(scanPtr, rollTime, holdTime) > now -> time)
                {
                    screen[scanPtr -> type]++;
                }
                scanPtr = scanPtr -> next;
            }
            
            /* 当前弹幕加入弹幕总数计数器或屏蔽弹幕计数器 */
            if(now -> type > 0 && now -> type <= 5)
            {
                count[now -> type]++;
            }
            else if(now -> type < 0 && now -> type >= -5)
            {
                block[-(now -> type)]++;
            }
            
            
            /* 开始更新表格数据 */
            if(now -> next != NULL)
            {/* 起始寻找时间上限是下一条弹幕的开始 */
                endTime = (now -> next) -> time;
            }
            else
            {/* 最后一次寻找时间上限是最后一条弹幕开始时间延迟30秒 */ 
                endTime = now -> time + 30;
            }
            
            if(fabs(now -> time - endTime) > EPS)
            {
                /* 更新屏蔽弹幕计数器与弹幕总数计数器 */
                count[0] = count[1] + count[2] + count[3] + count[4] + count[5];
                block[0] = block[1] + block[2] + block[3] + block[4] + block[5];
                for(cnt = 0; cnt <= NUM_OF_TYPE; cnt++)
                {
                    if(total[cnt])
                    {
                        printStatDataInt(opF, now -> time, endTime, 232, 65 + 30 * cnt, NULL, block[cnt]);
                        printStatDataInt(opF, now -> time, endTime, 317, 65 + 30 * cnt, NULL, count[cnt]);
                    }
                }
                
                /* 更新同屏弹幕计数器 注*两条弹幕之前可能有弹幕退出屏幕 */
                int sameTimeNum[6] = {0};/* 相同类型弹幕同时出现的次数计数器 */
                int minTimeType = 0;/* 最小时间对应的弹幕类型 */
                float debugTempTime, debugMinTime;
                startTime = endTime = now -> time;
                while(TRUE)
                {/* 在同屏弹幕中不断寻找结束时间的最小值，在这个时间点需要更新一条同屏弹幕-1的数据 */
                    scanPtr = signPtr;
                    arrset(sameTimeNum, 0, NUM_OF_TYPE + 1);
                    if(now -> next == NULL)
                    {/* 起始寻找时间上限是下一条弹幕的开始 */
                        debugMinTime = now -> time + 30;
                    }
                    else
                    {/* 最后一次寻找时间上限是最后一条弹幕开始时间延迟30秒 */
                        debugMinTime = (now -> next) -> time;
                    }
                    while(scanPtr != now -> next)
                    {/* 循环寻找最小时间 用于作本条记录结束时间 */
                        if((debugTempTime = getEndTime(scanPtr, rollTime, holdTime)) < EPS)
                        {/* 函数debugTempTime返回0.00表示弹幕发生了错误 */
                            scanPtr = scanPtr -> next;
                            continue;
                        }
                        
                        if(debugTempTime > endTime && debugTempTime < debugMinTime &&
                           ((now->next == NULL && debugTempTime < now->time + 30) || 
                            (now->next != NULL && debugTempTime < (now->next) -> time)) )
                        {/* debugMinTime > debugTempTime > endTime 且 debugTempTime在限定范围内 */
                            debugMinTime = debugTempTime;
                            
                            /* 最小值被刷新了，之前的相同类型弹幕统计也没有意义了 */
                            minTimeType = scanPtr -> type;
                            arrset(sameTimeNum, 0, NUM_OF_TYPE + 1);
                        }
                        if(debugMinTime == debugTempTime)
                        {
                            /* 统计同一时间消失的各类型弹幕数量 */
                            sameTimeNum[scanPtr -> type]++;
                        }
                        scanPtr = scanPtr -> next;
                    }
                    endTime = debugMinTime;
                    if(startTime == endTime)
                    {/* 找不出一个最小时间 且 值未改变 遍历结束 */
                        break;
                    }
                    
                    screen[0] = screen[1] + screen[2] + screen[3] + screen[4] + screen[5];
                    
                    if (density > 0)
                    {
                        char tempStr[32];
                        if (screen[0] > density)
                        {
                            sprintf(tempStr, "%d^/%d", screen[0], density); 
                        }
                        else
                        {
                            sprintf(tempStr, "%d/%d", screen[0], density); 
                        }
                        printStatDataStr(opF, startTime, endTime, 147, 65, NULL, tempStr);
                    }
                    else
                    {
                        printStatDataInt(opF, startTime, endTime, 147, 65, NULL, screen[0]);
                    }
                    
                    for(cnt = 1; cnt <= NUM_OF_TYPE; cnt++)
                    {/* 写数据 */
                        if(total[cnt])
                        {
                            printStatDataInt(opF, startTime, endTime,
                                              147, 65 + 30 * cnt, NULL, screen[cnt]);
                        }
                    }
                    startTime = endTime;/* 以本条记录的结束时间作为下一条记录的开始时间 */
                    for(cnt = 0; cnt <= NUM_OF_TYPE; cnt++)
                    {/* 更新同屏弹幕计数器 减去同一时间消失的弹幕 */ 
                        screen[cnt] -= sameTimeNum[cnt];
                    }
                }
            }
            
            now = now -> next;
        }/* 结束 while */

    }/* 结束 if */
    
    /* 提醒图表即将关闭 */
    if((mode & TABLE) || (mode & HISTOGRAM))
    {
        /* 提示文字底框进场动画 */
        fprintf(opF, "\nDialogue: 9,");
        printTime(opF, lastDanmakuStartTime, ",");
        printTime(opF, lastDanmakuStartTime + 0.20, ",");
        /* 底框 */
        if((mode & TABLE) && (mode & HISTOGRAM))
        {
            fprintf(opF, "danmakuFactory_stat,,0000,0000,0000,,{\\an4\\move(20,377,20,423)"
                         "\\iclip(m 20 340 l 445 340 l 445 385 l 20 385 l 20 340)\\1c&HEAF3CE"
                         "\\p1}m 15 0 l 410 0 b 416 0 425 9 425 15 l 425 30 b 425 36 416 45 410 45"
                         " l 15 45 b 9 45 0 37 0 30 l 0 15 b 0 9 9 0 15 0{\\p0}");
        }
        else if (mode & TABLE)
        {
            fprintf(opF, "danmakuFactory_stat,,0000,0000,0000,,{\\an4\\move(20,252,20,288)"
                         "\\iclip(m 20 245 l 445 470 l 445 515 l 20 515 l 20 470)\\1c&HEAF3CE"
                         "\\p1}m 15 0 l 410 0 b 416 0 425 9 425 15 l 425 30 b 425 36 416 45 410 45"
                         " l 15 45 b 9 45 0 37 0 30 l 0 15 b 0 9 9 0 15 0{\\p0}");
        }
        else if (mode & HISTOGRAM)
        {
            fprintf(opF, "Default,,0000,0000,0000,,{\\an4\\move(20,122,20,168)"
                         "\\iclip(m 20 85 l 445 85 l 445 130 l 20 130 l 20 85)\\1c&HEAF3CE"
                         "\\p1}m 15 0 l 410 0 b 416 0 425 9 425 15 l 425 30 b 425 36 416 45 410 45"
                         " l 15 45 b 9 45 0 37 0 30 l 0 15 b 0 9 9 0 15 0{\\p0}");
        }
        
        /* 提示文字底框固定后 */
        fprintf(opF, "\nDialogue: 9,");
        printTime(opF, lastDanmakuStartTime + 0.20, ",");
        printTime(opF, lastDanmakuStartTime + 30.00, ",");
        /* 底框 */
        if((mode & TABLE) && (mode & HISTOGRAM))
        {
            fprintf(opF, "danmakuFactory_stat,,0000,0000,0000,,{\\an7\\1c&HEAF3CE\\p1}"
                         "m 35 400 l 430 400 b 436 400 445 409 445 415 l 445 430 b 445 436 436 445 430 445"
                         " l 35 445 b 29 445 20 437 20 430 l 20 415 b 20 409 29 400 35 400{\\p0}");
        }
        else if (mode & TABLE)
        {
            fprintf(opF, "danmakuFactory_stat,,0000,0000,0000,,{\\an7\\1c&HEAF3CE\\p1}"
                         "m 35 275 l 430 275 b 436 275 445 284 445 290 l 445 305 b 445 311 436 320 430 320"
                         " l 35 320 b 29 320 20 312 20 305 l 20 290 b 20 280 29 275 35 275{\\p0}");
        }
        else if (mode & HISTOGRAM)
        {
            fprintf(opF, "danmakuFactory_stat,,0000,0000,0000,,{\\an7\\1c&HEAF3CE\\p1}"
                         "m 35 145 l 430 145 b 436 145 445 154 445 160 l 445 175 b 445 181 436 190 430 190"
                         " l 35 190 b 29 190 20 182 20 175 l 20 160 b 20 154 29 145 35 145{\\p0}");
        }
        
        /* 提示文字底框固定后 */
        fprintf(opF, "\nDialogue: 10,");
        printTime(opF, lastDanmakuStartTime + 0.20, ",");
        printTime(opF, lastDanmakuStartTime + 30.00, ",");
        /* 提示框进度条 */
        if((mode & TABLE) && (mode & HISTOGRAM))
        {
            fprintf(opF, "danmakuFactory_stat,,0000,0000,0000,,{\\an7\\move(20, 400, 445, 400)\\clip(m 35 400 l 430 400 "
                         "b 436 400 445 409 445 415 l 445 430 b 445 436 436 445 430 445 l 35 445 "
                         "b 29 445 20 437 20 430 l 20 415 b 20 409 29 400 35 400)"
                         "\\1c&HCECEF3\\p1}m 0 0 l -425 0 l -425 45 l 0 45 l 0 0{\\p0}");
        }
        else if (mode & TABLE)
        {
            fprintf(opF, "danmakuFactory_stat,,0000,0000,0000,,{\\an7\\move(20, 275, 445, 275)\\clip(m 35 275 l 430 275 "
                         "b 436 275 445 284 445 290 l 445 305 b 445 311 436 320 430 320 l 35 320 "
                         "b 29 320 20 312 20 305 l 20 290 b 20 280 29 275 35 275)"
                         "\\1c&HCECEF3\\p1}m 0 0 l -425 0 l -425 45 l 0 45 l 0 0{\\p0}");
        }
        else if (mode & HISTOGRAM)
        {
            fprintf(opF, "danmakuFactory_stat,,0000,0000,0000,,{\\an7\\move(20, 145, 445, 145)\\clip(m 35 145 l 430 145 "
                         "b 436 145 445 154 445 160 l 445 175 b 445 181 436 190 430 190 l 35 190 "
                         "b 29 190 20 182 20 175 l 20 160 b 20 154 29 145 35 145)"
                         "\\1c&HCECEF3\\p1}m 0 0 l -425 0 l -425 45 l 0 45 l 0 0{\\p0}");
        }
        
        /* 刷新退出时间 */
        for(cnt = 0; cnt < 30; cnt++)
        {
            fprintf(opF, "\nDialogue: 11,");
            printTime(opF, lastDanmakuStartTime + cnt, ",");
            printTime(opF, lastDanmakuStartTime + cnt + 1.00, ",");
            /* 底框 */
            if((mode & TABLE) && (mode & HISTOGRAM))
            {
                fprintf(opF, "danmakuFactory_stat,,0000,0000,0000,,{\\an4\\pos(35,422)\\b1\\fs25\\1c&HFFFFFF"
                             "}These charts will close after %d s", 30 - cnt);                
            }
            else if (mode & TABLE)
            {
                fprintf(opF, "danmakuFactory_stat,,0000,0000,0000,,{\\an4\\pos(35,297)\\b1\\fs25\\1c&HFFFFFF"
                             "}These charts will close after %d s", 30 - cnt); 
            }
            else if (mode & HISTOGRAM)
            {
                fprintf(opF, "danmakuFactory_stat,,0000,0000,0000,,{\\an4\\pos(35,167)\\b1\\fs25\\1c&HFFFFFF"
                             "}These charts will close after %d s", 30 - cnt);                    
            }
        }
    }
    fflush(opF);
    return 0;
}

/* 
 * 释放字幕结构体中动态分配的空间（结构体本身的空间不会被释放） 
  */
void freeAssFile(ASSFILE *assFile)
{
    if (assFile == NULL)
    {
        return;
    }
    
    EVENT *nextPtr, *nowPtr;
    nextPtr = nowPtr = assFile -> events;
    
    /* 释放样式表动态数组 */
    free(assFile -> styles);
    
    /* 释放事件链表 */ 
    while (nowPtr != NULL)
    {
        /* 释放文本部分动态数组 */
        free(nowPtr -> text);
        
        /* 记录下一个节点 并释放当前节点 */ 
        nextPtr = nowPtr -> next;
        free(nowPtr);
        
        nowPtr = nextPtr;
    }
    
    /* 置空 防止二次释放 */
    assFile -> styles = NULL; 
    assFile -> events = NULL;
}

/* 
 * 获取弹幕结束时间（秒）
 * 参数： 
 * 弹幕指针/滚动弹幕速度/固定弹幕保持时间 
 * 返回值：
 * 生存时间
 * 错误返回0.00 
 */
static float getEndTime(DANMAKU *danmakuPtr, const int rollTime, const int holdTime)
{/* [0,0.17,"1-1",7,"文本部分内容",0,0,0,0.17,500,0,true,"微软雅黑",1] */
    if (danmakuPtr == NULL)
    {
        return 0.00;
    }
    
    if(IS_R2L (danmakuPtr) || IS_L2R (danmakuPtr))
    {
        return danmakuPtr -> time + rollTime;
    }
    else if (IS_TOP(danmakuPtr) || IS_BTM (danmakuPtr))
    {
        return danmakuPtr -> time + holdTime;
    }
    else if (IS_SPECIAL(danmakuPtr))
    {/* 特殊弹幕需要取持续时间 */
        return danmakuPtr -> time + danmakuPtr -> special -> existTime;
    }
    else
    {
        return 0.00;
    }
}

/* 
 * 以ass格式标准打印时间 
 * 参数： 
 * 文件指针/秒数/紧接着打印在后面的字符串
 * 返回值：
 * ferror函数的返回值 
 */
static int printTime(FILE *filePtr, float time, const char *endText)
{
    if (time < EPS)
    {
        time = 0.0;
    }

    int hour = (int)time / 3600;
    time -= hour * 3600;
    int min = (int)time / 60;
    time -= min * 60;
    int sec = (int)time;
    int ms = (time - sec) * 100;
    fprintf(filePtr, "%01d:%02d:%02d.%02d%s", hour, min, sec, ms, endText);
    return ferror(filePtr);
}

/*
 * 获取移动指令字符串
 */
char *getActionStr(char *dstBuff,int shiftX, int shiftY, int startPosX, int startPosY, int endPosX, int endPosY)
{
    if (startPosX == endPosX && startPosY == endPosY)
    {
        sprintf(dstBuff, "\\pos(%d,%d)", startPosX+shiftX, startPosY+shiftY);
    }
    else
    {
        sprintf(dstBuff, "\\move(%d,%d,%d,%d)", startPosX+shiftX, startPosY+shiftY, endPosX+shiftX, endPosY+shiftY);
    }

    return dstBuff;
}

/*
 * 获取消息高度 
 */
int getMsgBoxHeight(DANMAKU *message, int fontSize, int width)
{
    int boxHeight = -1;
    int radius = fontSize / 2;
    if (message->type == MSG_GIFT)
    {
        boxHeight = fontSize;
    }
    else if (message->type == MSG_SUPER_CHAT)
    {
        int lineNum;
        int charCount = 0;
        unsigned char *textPtr = message->text;

        while (*textPtr != '\0')
        {
            if (*textPtr >= 0xC0 || *textPtr < 0x80)
            {/* 一个字符的开头 */
                charCount++;
            }
            textPtr++;
        }

        lineNum = (charCount * fontSize * SCBOX_TXT_LEN_COMPENSATION) / width + 1;

        int topBoxHeight = fontSize + fontSize*(4.0/5.0) + radius/2;
        int btmBoxHeight = lineNum * fontSize + radius/2;
        boxHeight = topBoxHeight + btmBoxHeight;
    }
    else if (message->type == MSG_GUARD)
    {
        boxHeight = fontSize + fontSize*(4.0/5.0) + radius;
    }

    return boxHeight;
}

/*
 * 打印一条消息到指定位置
 */
int printMessage(FILE *filePtr,
    int startPosX, int startPosY, int endPosX, int endPosY, float startTime, float endTime,
    int width, int fontSize, char *effect, DANMAKU *message)
{
    int radius = fontSize / 2;
    char actionStr[MAX_TEXT_LENGTH];
    
    if (message->type == MSG_GIFT)
    {
        fprintf(filePtr, "\nDialogue: 0,");
        printTime(filePtr, startTime, ",");
        printTime(filePtr, endTime, ",");
        fprintf(filePtr, "message_box,,0000,0000,0000,,{%s%s\\q2}{\\c&HBCACF7\\b1}%s: {\\c&HFFFFFF\\b0}%s x%d",
            getActionStr(actionStr, 0, 0, startPosX, startPosY, endPosX, endPosY), /* 移动指令 */
            effect, /* 补充特效 */
            message->user->name, message->gift->name, message->gift->count /* 礼物信息 */
        );
    }
    else if (message->type == MSG_SUPER_CHAT)
    {
        unsigned char scMsgStr[MAX_TEXT_LENGTH];
        unsigned char *srcStrPtr, *resStrPtr;
        srcStrPtr = message->text;
        resStrPtr = scMsgStr;

        /* 文本消息加入换行符 */
        int lineNum = 1;
        int charCount = 0;
        while (*srcStrPtr != '\0' && resStrPtr-scMsgStr < MAX_TEXT_LENGTH)
        {
            if (*srcStrPtr >= 0xC0 || *srcStrPtr < 0x80)
            {/* 一个字符的开头 */
                charCount++;
            }

            if (charCount * fontSize * SCBOX_TXT_LEN_COMPENSATION >= width)
            {/* 填入换行符 */
                *resStrPtr = '\\';
                resStrPtr++;
                *resStrPtr = 'N';
                resStrPtr++;

                lineNum++;
                charCount = 0;
            }

            *resStrPtr = *srcStrPtr;

            srcStrPtr++;
            resStrPtr++;
        }
        
        *resStrPtr = '\0';

        int topBoxHeight = fontSize + fontSize*(4.0/5.0) + radius/2;
        int btmBoxHeight = lineNum * fontSize + radius/2;

        /* 配色 */
        char topBoxColor[ASS_COLOR_LEN];
        char btmBoxColor[ASS_COLOR_LEN];
        char userIDColor[ASS_COLOR_LEN];
        char textColor[ASS_COLOR_LEN];

        strcpy(textColor, "\\c&H313131");

        if (message->gift->price + EPS > 1000)
        { /* 1k及以上 */
            strcpy(topBoxColor, "\\c&HE5E5FF");
            strcpy(btmBoxColor, "\\c&H8C8CF7");
            strcpy(userIDColor, "\\c&H0F0F75");
        }
        else if (message->gift->price + EPS > 500)
        { /* 500及以上 */
            strcpy(topBoxColor, "\\c&HD4F6FF");
            strcpy(btmBoxColor, "\\c&H8CCEF7");
            strcpy(userIDColor, "\\c&H236C64");
        }
        else if (message->gift->price + EPS > 100)
        { /* 100及以上 */
            strcpy(topBoxColor, "\\c&HCAF9F8");
            strcpy(btmBoxColor, "\\c&H76E8E9");
            strcpy(userIDColor, "\\c&H1A8B87");
        }
        else
        { /* 其他 */
            strcpy(topBoxColor, "\\c&HFCE8D8");
            strcpy(btmBoxColor, "\\c&HE4A47A");
            strcpy(userIDColor, "\\c&H8A3619");
        }

        /* 绘制上底框 */
        fprintf(filePtr, "\nDialogue: 0,");
        printTime(filePtr, startTime, ",");
        printTime(filePtr, endTime, ",");
        fprintf(filePtr, "message_box,,0000,0000,0000,,{%s%s%s\\shad0\\p1}m %d %d b %d %d %d %d %d %d "
            "l %d %d b %d %d %d %d %d %d l %d %d l %d %d",
            getActionStr(actionStr, 0, 0, startPosX, startPosY, endPosX, endPosY), /* 移动指令 */
            effect, /* 补充特效 */
            topBoxColor, /* 颜色 */
            0, radius, /* 起点 */
            0, radius / 2, radius / 2, 0, radius, 0, /* 左上圆角 */
            width - radius, 0, /* 上部直线 */
            width - radius/2, 0, width, radius/2, width, radius, /* 右上圆角 */
            width, topBoxHeight, /* 右边直线 */
            0, topBoxHeight /* 底线 */
        );

        /* 绘制下底框 */
        fprintf(filePtr, "\nDialogue: 0,");
        printTime(filePtr, startTime, ",");
        printTime(filePtr, endTime, ",");
        fprintf(filePtr, "message_box,,0000,0000,0000,,{%s%s\\shad0\\p1%s}m %d %d l %d %d l %d %d b %d %d %d %d %d %d l %d %d"
            "b %d %d %d %d %d %d",
            getActionStr(actionStr, 0, topBoxHeight, startPosX, startPosY, endPosX, endPosY), /* 移动指令 */
            effect, /* 补充特效 */
            btmBoxColor, /* 颜色 */
            0, 0, /* 起点 */
            width, 0, /* 上部直线 */
            width, btmBoxHeight-radius, /* 右边直线 */
            width, btmBoxHeight-radius/2, width-radius/2, btmBoxHeight, width-radius, btmBoxHeight, /* 右下圆角 */
            radius, btmBoxHeight, /* 底部直线 */
            radius/2, btmBoxHeight, 0, btmBoxHeight-radius/2, 0, btmBoxHeight-radius /* 左边直线 */
        );

        /* 用户ID */
        fprintf(filePtr, "\nDialogue: 1,");
        printTime(filePtr, startTime, ",");
        printTime(filePtr, endTime, ",");
        fprintf(filePtr, "message_box,,0000,0000,0000,,{%s%s%s\\fs%d\\b1\\q2}%s",
            getActionStr(actionStr, radius/2, radius/3, startPosX, startPosY, endPosX, endPosY), /* 移动指令 */
            effect, /* 补充特效 */
            userIDColor, /* 颜色 */
            fontSize, /* ID文字大小 */
            message->user->name /* 用户id */
        );

        /* SC金额 */
        fprintf(filePtr, "\nDialogue: 1,");
        printTime(filePtr, startTime, ",");
        printTime(filePtr, endTime, ",");
        fprintf(filePtr, "message_box,,0000,0000,0000,,{%s%s%s\\fs%d\\q2}SuperChat CNY %d",
            getActionStr(actionStr, radius/2, fontSize+radius/3, startPosX, startPosY, endPosX, endPosY), /* 移动指令 */
            effect, /* 补充特效 */
            textColor, /* 颜色 */
            (int)(fontSize*(4.0/5.0)), /* 金额文字大小 */
            (int)message->gift->price /* SC金额 */
        );

        /* SC内容 */
        fprintf(filePtr, "\nDialogue: 1,");
        printTime(filePtr, startTime, ",");
        printTime(filePtr, endTime, ",");
        fprintf(filePtr, "message_box,,0000,0000,0000,,{%s%s\\c&HFFFFFF\\q2}%s",
            getActionStr(actionStr, radius/2, topBoxHeight, startPosX, startPosY, endPosX, endPosY), /* 移动指令 */
            effect, /* 补充特效 */
            scMsgStr /* SC内容 */
        );
    }
    else if (message->type == MSG_GUARD)
    {
        int boxHeight = fontSize + fontSize*(4.0/5.0) + radius;
        
        /* 配色 */
        char boxColor[ASS_COLOR_LEN];
        char userIDColor[ASS_COLOR_LEN];
        char textColor[ASS_COLOR_LEN];

        strcpy(textColor, "\\c&H313131");
        if (message->gift->price + EPS > 19800)
        { /* 总督 */
            strcpy(boxColor, "\\c&HE5E5FF");
            strcpy(userIDColor, "\\c&H0F0F75");
        }
        else if (message->gift->price + EPS > 1980)
        { /* 提督 */
            strcpy(boxColor, "\\c&HCAF9F8");
            strcpy(userIDColor, "\\c&H1A8B87");
        }
        else
        { /* 舰长、未知 */
            strcpy(boxColor, "\\c&HFCE8D8");
            strcpy(userIDColor, "\\c&H8A3619");
        }

        /* 绘制底框 */
        fprintf(filePtr, "\nDialogue: 0,");
        printTime(filePtr, startTime, ",");
        printTime(filePtr, endTime, ",");
        fprintf(filePtr, "message_box,,0000,0000,0000,,{%s%s%s\\shad0\\p1}m %d %d b %d %d %d %d %d %d "
            "l %d %d b %d %d %d %d %d %d l %d %d b %d %d %d %d %d %d l %d %d b %d %d %d %d %d %d",
            getActionStr(actionStr, 0, 0, startPosX, startPosY, endPosX, endPosY), /* 移动指令 */
            effect, /* 补充特效 */
            boxColor, /* 颜色 */
            0, radius, /* 起点 */
            0, radius / 2, radius / 2, 0, radius, 0, /* 左上圆角 */
            width - radius, 0, /* 上部直线 */
            width - radius/2, 0, width, radius/2, width, radius, /* 右上圆角 */
            width, boxHeight-radius, /* 右边直线 */
            width, boxHeight-radius/2, width-radius/2, boxHeight, width-radius, boxHeight, /* 右下圆角 */
            radius, boxHeight, /* 底线 */
            radius/2, boxHeight, 0, boxHeight-radius/2, 0, boxHeight-radius /* 左下圆角 */
        );

        /* 用户ID */
        fprintf(filePtr, "\nDialogue: 1,");
        printTime(filePtr, startTime, ",");
        printTime(filePtr, endTime, ",");
        fprintf(filePtr, "message_box,,0000,0000,0000,,{%s%s%s\\fs%d\\q2}%s",
            getActionStr(actionStr, radius/2, radius/3, startPosX, startPosY, endPosX, endPosY), /* 移动指令 */
            effect, /* 补充特效 */
            userIDColor, /* 颜色 */
            fontSize, /* ID文字大小 */
            message->user->name /* 用户id */
        );

        /* 舰长信息 */
        fprintf(filePtr, "\nDialogue: 1,");
        printTime(filePtr, startTime, ",");
        printTime(filePtr, endTime, ",");
        fprintf(filePtr, "message_box,,0000,0000,0000,,{%s%s%s\\fs%d\\q2}Welcome new %s!",
            getActionStr(actionStr, radius/2, fontSize+radius/3, startPosX, startPosY, endPosX, endPosY), /* 移动指令 */
            effect, /* 补充特效 */
            textColor, /* 颜色 */
            (int)(fontSize*(4.0/5.0)), /* 舰长信息文字大小 */
            message->gift->name /* 礼物名称 */
        );
    }
    
    return ferror(filePtr);
}

/* 
 * 寻找最小值 
 * 参数：
 * 欲找最小值的数组/成员数/终止下标/模式（0正序，1逆序）
 * 返回值：
 * 最小值数组下标 
  */
static int findMin(float *array, const int numOfLine, const int stopSubScript, const int mode)
{
    int cnt, minSub;
    if(!mode)
    {/* 正序查找 */
        minSub = 0;
        for(cnt = 0; cnt < stopSubScript; cnt++)
        {
            if(array[cnt] < array[minSub])
            {
                minSub = cnt;
            }
        }
    }
    else
    {/* 逆序查找 */ 
        minSub = numOfLine - 1;
        for(cnt = numOfLine - 1; cnt >= stopSubScript; cnt--)
        {
            if(array[cnt] < array[minSub])
            {
                minSub = cnt;
            }
        }
    }
    return minSub;
}

/* 
 * 根据信息打印stat数据表上的信息（整数） 
 * 参数： 
 * 文件指针/开始秒数/结束秒数/强制定位x/强制定位Y/特效追加/整型数据
 * 返回值：
 * ferror函数的返回值 
  */
static int printStatDataInt(FILE *filePtr, const float startTime, const float endTime, const int posX,
                                const int posY, char *effect, const int data)
{
    fprintf(filePtr, "\nDialogue:10,");
    printTime(filePtr, startTime, ","); 
    printTime(filePtr, endTime, ",");
    fprintf(filePtr, "danmakuFactory_stat,,0000,0000,0000,,{\\pos(%d,%d)\\b1", posX, posY);
    if(effect != NULL)
    {
        fprintf(filePtr, "%s", effect);
    }
    fprintf(filePtr, "\\fs25\\1c&HFFFFFF}%d", data);
    return ferror(filePtr);
}

/* 
 * 根据信息打印stat数据表上的信息（字符串） 
 * 参数： 
 * 文件指针/开始秒数/结束秒数/强制定位x/强制定位Y/特效追加/字符串 
 * 返回值：
 * ferror函数的返回值 
  */
static int printStatDataStr(FILE *filePtr, const float startTime, const float endTime, const int posX,
                        const int posY, const char *effect, const char *str)
{
    fprintf(filePtr, "\nDialogue:10,");
    printTime(filePtr, startTime, ",");
    printTime(filePtr, endTime, ",");
    fprintf(filePtr, "danmakuFactory_stat,,0000,0000,0000,,{\\pos(%d,%d)\\b1", posX, posY);
    if(effect != NULL)
    {
        fprintf(filePtr, "%s", effect);
    }
    fprintf(filePtr, "\\fs25\\1c&HFFFFFF}%s", str);
    return ferror(filePtr);
}
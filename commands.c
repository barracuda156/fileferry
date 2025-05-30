#include "commands.h"
#include "ui.h"
#include "settings.h"
#include "help.h"
#include "file_include_exclude.h"
#include "errors_and_logging.h"
#include "filestore.h"
#include "filestore_dirlist.h"

char *CommandFileLoad(char *RetStr, const char *Path)
{
    STREAM *S;
    char *Tempstr=NULL;

    S=STREAMOpen(Path, "r");
    if (S)
    {
        Tempstr=STREAMReadLine(Tempstr, S);
        while (Tempstr)
        {
            StripTrailingWhitespace(Tempstr);
            RetStr=MCatStr(RetStr, Tempstr, "; ", NULL);
            Tempstr=STREAMReadLine(Tempstr, S);
        }
        STREAMClose(S);
    }

    Destroy(Tempstr);
    return(RetStr);
}


//this function gets a command argument that is intended as a file-extension
//if it doesn't start with '.' then '.' is added
const char *CommandLineGetExtn(const char *CommandLine, char **Extn)
{
    char *Token=NULL;
    const char *ptr;

    ptr=GetToken(CommandLine, "\\S", &Token, GETTOKEN_QUOTES);
    if (*Token != '.') *Extn=MCopyStr(*Extn, ".", Token, NULL);
    else *Extn=CopyStr(*Extn, Token);

    Destroy(Token);
    return(ptr);
}


//CommandLine here is command line after the switch
const char *ParseCommandSwitch(const char *CommandLine, TCommand *Cmd, const char *Switch)
{
    char *Token=NULL, *Tempstr=NULL;
    const char *ptr;
    long val;

    if (strcmp(Switch, "-a")==0) Cmd->Flags |= CMD_FLAG_ALL;
    else if (strcmp(Switch, "-A")==0) Cmd->Flags |= CMD_FLAG_ABORT;
    else if (strcmp(Switch, "-Q")==0) Cmd->Flags |= CMD_FLAG_QUIT;
    else if (strcmp(Switch, "-F")==0) Cmd->Flags |= CMD_FLAG_FORCE;
    else if (strcmp(Switch, "-r")==0) Cmd->Flags |= CMD_FLAG_RECURSE;
    else if (strcmp(Switch, "-S")==0) Cmd->Flags |= CMD_FLAG_SORT_SIZE;
    else if (strcmp(Switch, "-page")==0) Cmd->Flags |= CMD_FLAG_PAGE;
    else if (strcmp(Switch, "-pg")==0) Cmd->Flags |= CMD_FLAG_PAGE;
    else if (strcmp(Switch, "-files")==0) Cmd->Flags |= CMD_FLAG_FILES_ONLY;
    else if (strcmp(Switch, "-dirs")==0) Cmd->Flags |= CMD_FLAG_DIRS_ONLY;
    else if (strcmp(Switch, "-file")==0) Cmd->Flags |= CMD_FLAG_FILES_ONLY;
    else if (strcmp(Switch, "-dir")==0) Cmd->Flags |= CMD_FLAG_DIRS_ONLY;
    else if (strcmp(Switch, "-i")==0)
    {
        CommandLine=GetToken(CommandLine, "\\S", &Token, GETTOKEN_QUOTES);
        Cmd->Includes=MCatStr(Cmd->Includes, "'",  Token, "' ", NULL);
    }
    else if (strcmp(Switch, "-x")==0)
    {
        CommandLine=GetToken(CommandLine, "\\S", &Token, GETTOKEN_QUOTES);
        Cmd->Excludes=MCatStr(Cmd->Excludes, "'",  Token, "' ", NULL);
    }
    else if (strcmp(Switch, "-newer")==0)
    {
        Cmd->Flags |= CMD_FLAG_NEWER;
        if ((Cmd->Type != CMD_DIFF) && (Cmd->Type != CMD_CMP))
        {
            CommandLine=GetToken(CommandLine, "\\S", &Token, GETTOKEN_QUOTES);
            if (! StrValid(Token)) Token=CopyStr(Token, "?");
            SetVar(Cmd->Vars, "Time:Newer", Token);
        }
    }
    else if (strcmp(Switch, "-older")==0)
    {
        Cmd->Flags |= CMD_FLAG_OLDER;
        if ((Cmd->Type != CMD_DIFF) && (Cmd->Type != CMD_CMP))
        {
            CommandLine=GetToken(CommandLine, "\\S", &Token, GETTOKEN_QUOTES);
            if (! StrValid(Token)) Token=CopyStr(Token, "?");
            SetVar(Cmd->Vars, "Time:Older", Token);
        }
    }
    else if (strcmp(Switch, "-mtime")==0)
    {
        CommandLine=GetToken(CommandLine, "\\S", &Token, GETTOKEN_QUOTES);
        val=atoi(Token);
        Token=CopyStr(Token, GetDateStrFromSecs("%Y/%m/%d %H:%M:%S", Now+val, NULL));

        if (val < 0) SetVar(Cmd->Vars, "Time:Newer", Token);
        else SetVar(Cmd->Vars, "Time:Older", Token);
    }
    else if (strcmp(Switch, "-larger")==0)
    {
        CommandLine=GetToken(CommandLine, "\\S", &Token, GETTOKEN_QUOTES);
        SetVar(Cmd->Vars, "Size:Larger", Token);
    }
    else if (strcmp(Switch, "-smaller")==0)
    {
        CommandLine=GetToken(CommandLine, "\\S", &Token, GETTOKEN_QUOTES);
        SetVar(Cmd->Vars, "Size:Smaller", Token);
    }


    switch (Cmd->Type)
    {
    case CMD_LS:
    case CMD_LLS:
        if (strcmp(Switch, "-t")==0) Cmd->Flags |= CMD_FLAG_SORT_TIME;
        else if (strcmp(Switch, "-lt")==0) Cmd->Flags |= CMD_FLAG_SORT_TIME | CMD_FLAG_LONG;
        else if (strcmp(Switch, "-f")==0) Cmd->Flags |= CMD_FLAG_FILES_ONLY;
        else if (strcmp(Switch, "-l")==0) Cmd->Flags |= CMD_FLAG_LONG;
        else if (strcmp(Switch, "-ll")==0) Cmd->Flags |= CMD_FLAG_LONG | CMD_FLAG_LONG_LONG;
        else if (strcmp(Switch, "-d")==0) Cmd->Flags |= CMD_FLAG_DIRS_ONLY;
        else if (strcmp(Switch, "-n")==0)
        {
            CommandLine=GetToken(CommandLine, "\\S", &Token, GETTOKEN_QUOTES);
            Cmd->NoOfItems = atoi(Token);
        }
        else if ( (strcmp(Switch, "-k")==0) || (strcmp(Switch, "-keyword")==0) )
        {
            CommandLine=GetToken(CommandLine, "\\S", &Token, GETTOKEN_QUOTES);
            Tempstr=MCopyStr(Tempstr, "'", Token, "' ", NULL);
            AppendVar(Cmd->Vars, "Keywords", Tempstr);
        }
        break;

    case CMD_EXISTS:
    case CMD_LEXISTS:
        if (strcmp(Switch, "-f")==0) Cmd->Flags |= CMD_FLAG_FILES_ONLY;
        else if (strcmp(Switch, "-d")==0) Cmd->Flags |= CMD_FLAG_DIRS_ONLY;
        else if (strcmp(Switch, "-no")==0) Cmd->Flags |= CMD_FLAG_INVERT;
        break;

    case CMD_LOCK:
    case CMD_LLOCK:
        if (strcmp(Switch, "-w")==0) Cmd->Flags |= CMD_FLAG_WAIT;
        else if (strcmp(Switch, "-wait")==0) Cmd->Flags |= CMD_FLAG_WAIT;
        break;

    case CMD_SHOW:
        //special case where sixel can be set at the session-wide level
        if (Settings->Flags & SETTING_SIXEL) Cmd->Flags |= CMD_FLAG_SIXEL;

        if (strcmp(Switch, "-img")==0) Cmd->Flags |= CMD_FLAG_IMG;
        else if (strcmp(Switch, "-sixel")==0) Cmd->Flags |= CMD_FLAG_SIXEL;
        else if (strcmp(Switch, "-thumb")==0) Cmd->Flags |= CMD_FLAG_THUMB;
        break;

    case CMD_PUT:
    case CMD_MPUT:
        if (strcmp(Switch, "-expire")==0)
        {
            CommandLine=GetToken(CommandLine, " ", &Token, GETTOKEN_QUOTES);
            SetVar(Cmd->Vars, "Expire", Token);
        }
    //fall through to GET
    case CMD_GET:
    case CMD_MGET:
        if (strcmp(Switch,"-s")==0) Cmd->Flags |= CMD_FLAG_SYNC;
        else if (strcmp(Switch, "-f")==0) Cmd->Flags |= CMD_FLAG_FORCE;
        else if (strcmp(Switch, "-I")==0) Cmd->Flags |= CMD_FLAG_INTEGRITY;
        else if (strcmp(Switch, "-integrity")==0) Cmd->Flags |= CMD_FLAG_INTEGRITY;
        else if (strcmp(Switch, "-n")==0)
        {
            CommandLine=GetToken(CommandLine, "\\S", &Token, GETTOKEN_QUOTES);
            Cmd->NoOfItems = atoi(Token);
        }
        else if (strcmp(Switch,"-enc")==0) Cmd->EncryptType = ENCRYPT_ANY;
        else if (strcmp(Switch,"-ssl")==0) Cmd->EncryptType = ENCRYPT_OPENSSL_PW;
        else if (strcmp(Switch,"-sslenc")==0) Cmd->EncryptType = ENCRYPT_OPENSSL_PW;
        else if (strcmp(Switch,"-gpg")==0) Cmd->EncryptType = ENCRYPT_GPG_PW;
        else if (strcmp(Switch,"-zenc")==0) Cmd->EncryptType = ENCRYPT_ZIP;
        else if (strcmp(Switch,"-7zenc")==0) Cmd->EncryptType = ENCRYPT_7ZIP;
        else if (strcmp(Switch, "-resume")==0) Cmd->Flags |= CMD_FLAG_RESUME;
        else if (strcmp(Switch, "-t")==0) SetVar(Cmd->Vars, "DestTransferExtn", ".tmp");
        else if ( (strcmp(Switch, "-k")==0) || (strcmp(Switch, "-keyword")==0) )
        {
            CommandLine=GetToken(CommandLine, "\\S", &Token, GETTOKEN_QUOTES);
            Tempstr=MCopyStr(Tempstr, "'", Token, "' ", NULL);
            AppendVar(Cmd->Vars, "Keywords", Tempstr);
        }
        else if (strcmp(Switch, "-h")==0)
        {
            CommandLine=GetToken(CommandLine, " ", &Token, GETTOKEN_QUOTES);
            SetVar(Cmd->Vars, "PostTransferHookScript", Token);
        }
        else if (strcmp(Switch, "-posthook")==0)
        {
            CommandLine=GetToken(CommandLine, " ", &Token, GETTOKEN_QUOTES);
            SetVar(Cmd->Vars, "PostTransferHookScript", Token);
        }
        else if (strcmp(Switch, "-prehook")==0)
        {
            CommandLine=GetToken(CommandLine, " ", &Token, GETTOKEN_QUOTES);
            SetVar(Cmd->Vars, "PreTransferHookScript", Token);
        }
        else if (strcmp(Switch, "-ed")==0)
        {
            CommandLine=CommandLineGetExtn(CommandLine, &Token);
            SetVar(Cmd->Vars, "DestFinalExtn", Token);
        }
        else if (strcmp(Switch, "-es")==0)
        {
            CommandLine=CommandLineGetExtn(CommandLine, &Token);
            SetVar(Cmd->Vars, "SourceFinalExtn", Token);
        }
        else if (strcmp(Switch, "-et")==0)
        {
            CommandLine=CommandLineGetExtn(CommandLine, &Token);
            SetVar(Cmd->Vars, "DestTransferExtn", Token);
        }
        else if (strcmp(Switch, "-Ed")==0)
        {
            CommandLine=CommandLineGetExtn(CommandLine, &Token);
            SetVar(Cmd->Vars, "DestAppendExtn", Token);
        }
        else if (strcmp(Switch, "-Es")==0)
        {
            CommandLine=CommandLineGetExtn(CommandLine, &Token);
            SetVar(Cmd->Vars, "SourceAppendExtn", Token);
        }
        else if (strcmp(Switch, "-key")==0)
        {
            CommandLine=GetToken(CommandLine, " ", &Token, GETTOKEN_QUOTES);
            Cmd->EncryptKey=CopyStr(Cmd->EncryptKey, Token);
        }
        else if (strcmp(Switch, "-bak")==0)
        {
            CommandLine=GetToken(CommandLine, " ", &Token, GETTOKEN_QUOTES);
            SetVar(Cmd->Vars, "DestBackups", Token);
        }
        break;
    }

    Destroy(Tempstr);
    Destroy(Token);

    return(CommandLine);
}



int CommandMatch(const char *Str)
{
    int Cmd=CMD_NONE;

    if (StrValid(Str))
    {
        if (strcmp(Str, "cd")==0) Cmd=CMD_CD;
        else if (strcmp(Str, "chdir")==0) Cmd=CMD_CD;
        else if (strcmp(Str, "lcd")==0) Cmd=CMD_LCD;
        else if (strcmp(Str, "lchdir")==0) Cmd=CMD_LCD;
        else if (strcmp(Str, "mkdir")==0) Cmd=CMD_MKDIR;
        else if (strcmp(Str, "lmkdir")==0) Cmd=CMD_LMKDIR;
        else if (strcmp(Str, "rmdir")==0) Cmd=CMD_RMDIR;
        else if (strcmp(Str, "lrmdir")==0) Cmd=CMD_LRMDIR;
        else if (strcmp(Str, "rm")==0)  Cmd=CMD_DEL;
        else if (strcmp(Str, "lrm")==0)  Cmd=CMD_LDEL;
        else if (strcmp(Str, "del")==0) Cmd=CMD_DEL;
        else if (strcmp(Str, "ldel")==0) Cmd=CMD_LDEL;
        else if (strcmp(Str, "ls")==0)  Cmd=CMD_LS;
        else if (strcmp(Str, "lls")==0) Cmd=CMD_LLS;
        else if (strcmp(Str, "stat")==0)  Cmd=CMD_STAT;
        else if (strcmp(Str, "lstat")==0) Cmd=CMD_LSTAT;
        else if (strcmp(Str, "stats")==0)  Cmd=CMD_STAT;
        else if (strcmp(Str, "lstats")==0) Cmd=CMD_LSTAT;
        else if (strcmp(Str, "get")==0) Cmd=CMD_GET;
        else if (strcmp(Str, "put")==0) Cmd=CMD_PUT;
        else if (strcmp(Str, "mget")==0) Cmd=CMD_MGET;
        else if (strcmp(Str, "mput")==0) Cmd=CMD_MPUT;
        else if (strcmp(Str, "show")==0) Cmd=CMD_SHOW;
        else if (strcmp(Str, "lshow")==0) Cmd=CMD_LSHOW;
        else if (strcmp(Str, "share")==0) Cmd=CMD_SHARE;
        else if (strcmp(Str, "pwd")==0) Cmd=CMD_PWD;
        else if (strcmp(Str, "lpwd")==0) Cmd=CMD_LPWD;
        else if (strcmp(Str, "cp")==0) Cmd=CMD_COPY;
        else if (strcmp(Str, "lcp")==0) Cmd=CMD_LCOPY;
        else if (strcmp(Str, "copy")==0) Cmd=CMD_COPY;
        else if (strcmp(Str, "lcopy")==0) Cmd=CMD_LCOPY;
        else if (strcmp(Str, "ln")==0) Cmd=CMD_LINK;
        else if (strcmp(Str, "lln")==0) Cmd=CMD_LLINK;
        else if (strcmp(Str, "link")==0) Cmd=CMD_LINK;
        else if (strcmp(Str, "llink")==0) Cmd=CMD_LLINK;
        else if (strcmp(Str, "mv")==0) Cmd=CMD_RENAME;
        else if (strcmp(Str, "lmv")==0) Cmd=CMD_LRENAME;
        else if (strcmp(Str, "move")==0) Cmd=CMD_RENAME;
        else if (strcmp(Str, "lmove")==0) Cmd=CMD_LRENAME;
        else if (strcmp(Str, "rename")==0) Cmd=CMD_RENAME;
        else if (strcmp(Str, "chext")==0) Cmd=CMD_CHEXT;
        else if (strcmp(Str, "lchext")==0) Cmd=CMD_LCHEXT;
        else if (strcmp(Str, "chmod")==0) Cmd=CMD_CHMOD;
        else if (strcmp(Str, "crc")==0) Cmd=CMD_CRC;
        else if (strcmp(Str, "lcrc")==0) Cmd=CMD_LCRC;
        else if (strcmp(Str, "md5")==0) Cmd=CMD_MD5;
        else if (strcmp(Str, "md5sum")==0) Cmd=CMD_MD5;
        else if (strcmp(Str, "lmd5")==0) Cmd=CMD_LMD5;
        else if (strcmp(Str, "lmd5sum")==0) Cmd=CMD_LMD5;
        else if (strcmp(Str, "sha1")==0) Cmd=CMD_SHA1;
        else if (strcmp(Str, "sha1sum")==0) Cmd=CMD_SHA1;
        else if (strcmp(Str, "lsha1")==0) Cmd=CMD_LSHA1;
        else if (strcmp(Str, "lsha1sum")==0) Cmd=CMD_LSHA1;
        else if (strcmp(Str, "sha256")==0) Cmd=CMD_SHA256;
        else if (strcmp(Str, "sha256sum")==0) Cmd=CMD_SHA256;
        else if (strcmp(Str, "lsha256")==0) Cmd=CMD_LSHA256;
        else if (strcmp(Str, "lsha256sum")==0) Cmd=CMD_LSHA256;
        else if (strcmp(Str, "info")==0) Cmd=CMD_INFO;
        else if (strcmp(Str, "df")==0) Cmd=CMD_DISK_FREE;
        else if (strcmp(Str, "exist")==0) Cmd=CMD_EXISTS;
        else if (strcmp(Str, "lexist")==0) Cmd=CMD_LEXISTS;
        else if (strcmp(Str, "exists")==0) Cmd=CMD_EXISTS;
        else if (strcmp(Str, "lexists")==0) Cmd=CMD_LEXISTS;
        else if (strcmp(Str, "lock")==0) Cmd=CMD_LOCK;
        else if (strcmp(Str, "llock")==0) Cmd=CMD_LLOCK;
        else if (strcmp(Str, "unlock")==0) Cmd=CMD_UNLOCK;
        else if (strcmp(Str, "lunlock")==0) Cmd=CMD_LUNLOCK;
        else if (strcmp(Str, "chpassword")==0) Cmd=CMD_CHPASSWORD;
        else if (strcmp(Str, "password")==0) Cmd=CMD_CHPASSWORD;
        else if (strcmp(Str, "chpasswd")==0) Cmd=CMD_CHPASSWORD;
        else if (strcmp(Str, "passwd")==0) Cmd=CMD_CHPASSWORD;
        else if (strcmp(Str, "diff")==0) Cmd=CMD_DIFF;
        else if (strcmp(Str, "cmp")==0) Cmd=CMD_CMP;
        else if (strcmp(Str, "compare")==0) Cmd=CMD_CMP;
        else if (strcmp(Str, "hcmp")==0) Cmd=CMD_HCMP;
        else if (strcmp(Str, "set")==0) Cmd=CMD_SET;
        else if (strcmp(Str, "help")==0) Cmd=CMD_HELP;
        else if (strcmp(Str, "quit")==0) Cmd=CMD_QUIT;
        else if (strcmp(Str, "exit")==0) Cmd=CMD_QUIT;
        else Cmd=CMD_UNKNOWN;
    }

    return(Cmd);
}


TCommand *CommandParse(const char *Str)
{
    TCommand *Cmd;
    char *Token=NULL;
    const char *ptr;
    int SwitchesActive=TRUE;

    Cmd=(TCommand *) calloc(1, sizeof(TCommand));
    Cmd->Vars=ListCreate();
    ptr=GetToken(Str, "\\S", &Token, 0);
    Cmd->Type=CommandMatch(Token);

    switch (Cmd->Type)
    {
    //some commands have no switches and only take one argument
    case CMD_CD:
    case CMD_LCD:
        Cmd->Target=CopyStr(Cmd->Target, ptr);
        StripDirectorySlash(Cmd->Target);
        break;

    case CMD_SET:
        ptr=GetToken(ptr, "\\S", &Token, 0);
        if (! StrValid(Token)) UI_ShowSettings();
        else SettingChange(Token, ptr);
        break;

    case CMD_UNKNOWN:
        HandleEvent(NULL, UI_OUTPUT_ERROR, "unrecognized command: $(path)", Token, "");
        break;


    default:
        ptr=GetToken(ptr, "\\S", &Token, GETTOKEN_QUOTES);
        while (ptr)
        {
            if ((*Token == '-') && SwitchesActive)
            {
                if (strcmp(Token, "--")==0) SwitchesActive=FALSE;
                else ptr=ParseCommandSwitch(ptr, Cmd, Token);
            }
            else //deal with arguments that AREN'T option switches
            {
                switch (Cmd->Type)
                {
                //some commands have many file arguments
                case CMD_MPUT:
                case CMD_MGET:
                    Cmd->Target=MCatStr(Cmd->Target, "'", Token, "' ", NULL);
                    break;

                //some commands have one 'change' argument (file mode or extension) and many file arguments
                case CMD_CHMOD:
                case CMD_CHEXT:
                    if (StrValid(Cmd->Mode)) Cmd->Target=MCatStr(Cmd->Target, "'", Token, "' ", NULL);
                    else Cmd->Mode=CopyStr(Cmd->Mode, Token);
                    break;

                //some commands have 'src' and 'dest'
                case CMD_COPY:
                case CMD_LCOPY:
                case CMD_RENAME:
                case CMD_LRENAME:
                case CMD_LINK:
                case CMD_LLINK:
                case CMD_CHPASSWORD:
                    if (StrValid(Cmd->Target)) Cmd->Dest=CopyStr(Cmd->Dest, Token);
                    else Cmd->Target=CopyStr(Cmd->Target, Token);
                    break;


                //default is for commands to have switches and ONE argument
                default:
                    if (StrValid(Cmd->Target)) Cmd->Target=MCatStr(Cmd->Target, " ", Token, NULL);
                    else Cmd->Target=CopyStr(Cmd->Target, Token);
                    break;
                }
            }
            ptr=GetToken(ptr, "\\S", &Token, GETTOKEN_QUOTES);
        }
        break;
    }

    Destroy(Token);
    return(Cmd);
}


void CommandActivateTimeout()
{
    if (Settings->CommandTimeout > 0) alarm(Settings->CommandTimeout);
    else if (Settings->ProcessTimeout > 0) alarm(Settings->ProcessTimeout);
}

void CommandDeactivateTimeout()
{
    alarm(0);
}


#define CMD_ABORT -1

#define CMD_TYPE_PATH 1
#define CMD_TYPE_DEST 2
#define CMD_TYPE_ITEM 3
#define CMD_TYPE_MODE 4

int CommandGlobAndProcess(TFileStore *FS, int CmdType, TCommand *Cmd, void *Func)
{
    ListNode *DirList, *Curr;
    TFileItem *FI;
    int result=FALSE;

    DirList=FileStoreGlob(FS, Cmd->Target);
    Curr=ListGetNext(DirList);
    if (! Curr) UI_Output(UI_OUTPUT_ERROR, "No files matching '%s'", Cmd->Target);

    while (Curr)
    {
        FI=(TFileItem *) Curr->Item;
        if (FileIncluded(Cmd, FI, FS, FS))
        {
            result=TRUE;
            switch (CmdType)
            {
            case CMD_TYPE_PATH:
                result=((PATH_FUNC) Func)(FS, FI->path);
                break;
            case CMD_TYPE_ITEM:
                result=((ITEM_FUNC) Func)(FS, FI);
                break;
            case CMD_TYPE_DEST:
                result=((RENAME_FUNC) Func)(FS, FI->path, Cmd->Dest);
                break;
            case CMD_TYPE_MODE:
                //we use 'RENAME_FUNC' because that accepts two strings
                result=((RENAME_FUNC) Func)(FS, Cmd->Mode, FI->path);
                break;
            }
            if ((! result) && (Cmd->Flags & CMD_FLAG_ABORT)) return(CMD_ABORT);
        }
        Curr=ListGetNext(Curr);
    }

    FileStoreDirListFree(FS, DirList);

    return(result);
}



static void CommandGetValueGlob(TFileStore *FS, const char *ValueName, TCommand *Cmd)
{
    ListNode *DirList, *Curr;
    char *Tempstr=NULL;
    TFileItem *FI;

    if (FS->GetValue)
    {
        DirList=FileStoreGlob(FS, Cmd->Target);
        Curr=ListGetNext(DirList);
        if (! Curr) UI_Output(UI_OUTPUT_ERROR, "No files matching '%s'", Cmd->Target);

        while (Curr)
        {
            FI=(TFileItem *) Curr->Item;
            if (FileIncluded(Cmd, FI, FS, FS))
            {
                Tempstr=FS->GetValue(Tempstr, FS, FI->path, ValueName);
                printf("%s 	%s\n", Tempstr, FI->name);
            }
            Curr=ListGetNext(Curr);
        }

        FileStoreDirListFree(FS, DirList);
    }

    Destroy(Tempstr);
}



int CommandHashCompare(TFileStore *LocalFS, TFileStore *RemoteFS, const char *LocalPath, const char *RemotePath)
{
    char *Token=NULL, *RemoteHash=NULL, *LocalHash=NULL;
    char *Tempstr=NULL;
    const char *ptr, *p_RemoteItem;
    int result=TRUE;

    ptr=GetVar(RemoteFS->Vars, "HashTypes");
    if (! StrValid(ptr)) return(TRUE);

    if (RemotePath) p_RemoteItem=RemotePath;
    else p_RemoteItem=LocalPath;

    ptr=GetToken(ptr, " ", &Token, GETTOKEN_QUOTES);
    //while (ptr)
    {
        if (LocalFS->GetValue && RemoteFS->GetValue)
        {
            LocalHash=LocalFS->GetValue(LocalHash, LocalFS, LocalPath, Token);
            RemoteHash=RemoteFS->GetValue(RemoteHash, RemoteFS, p_RemoteItem, Token);
            if (CompareStrNoCase(RemoteHash, LocalHash) != 0) result=FALSE;

            if (result==TRUE) Tempstr=CopyStr(Tempstr, "~g");
            else Tempstr=CopyStr(Tempstr, "~r");

            UI_Output(0, "%s%s~0 local:%s:%s  remote:%s:%s\n", Tempstr, Token, LocalPath, LocalHash, p_RemoteItem, RemoteHash);
        }
        //  ptr=GetToken(ptr, " ", &Token, GETTOKEN_QUOTES);
    }

    Destroy(RemoteHash);
    Destroy(LocalHash);
    Destroy(Tempstr);
    Destroy(Token);

    return(result);
}


void CommandDiff(TCommand *Cmd, TFileStore *LocalFS, TFileStore *RemoteFS)
{
    ListNode *LocalDir, *RemoteDir, *Curr, *Node;
    TFileItem *LocalItem, *RemoteItem;
    char *RemoteTime=NULL, *LocalTime=NULL, *MatchType=NULL;
    int diff=0, match=0;
    int result;

    if (! StrValid(Cmd->Target))
    {
        UI_Output(UI_OUTPUT_ERROR, "No target given for diff command.");
        return;
    }

    LocalDir=FileStoreGlob(LocalFS, Cmd->Target);
    RemoteDir=FileStoreGlob(RemoteFS, Cmd->Target);

    Curr=ListGetNext(LocalDir);
    while (Curr)
    {
        LocalItem=(TFileItem *) Curr->Item;
        Node=ListFindNamedItem(RemoteDir, Curr->Tag);
        if (Node)
        {
            RemoteItem=(TFileItem *) Node->Item;

            result=FileStoreCompareFileItems(LocalFS, RemoteFS, LocalItem, RemoteItem, &MatchType);

            if (result == CMP_LOCAL_NEWER)
            {
                if (! (Cmd->Flags & CMD_FLAG_OLDER)) result=CMP_MATCH;
            }

            if (result == CMP_REMOTE_NEWER)
            {
                if (! (Cmd->Flags & CMD_FLAG_NEWER)) result=CMP_MATCH;
            }


            switch (result)
            {
            case CMP_SIZE_MISMATCH:
                printf("%-30s   size difference   local:%llu    remote:%llu\n", Curr->Tag, (long long unsigned int) LocalItem->size, (long long unsigned int)  RemoteItem->size);
                diff++;
                break;

            case CMP_LOCAL_NEWER:
                LocalTime=CopyStr(LocalTime, GetDateStrFromSecs("%Y-%m-%dT%H:%M:%S", LocalItem->mtime, NULL));
                RemoteTime=CopyStr(RemoteTime, GetDateStrFromSecs("%Y-%m-%dT%H:%M:%S", RemoteItem->mtime, NULL));
                printf("%-30s   local newer       local:%s    remote:%s\n", Curr->Tag, LocalTime, RemoteTime);
                diff++;
                break;

            case CMP_REMOTE_NEWER:
                LocalTime=CopyStr(LocalTime, GetDateStrFromSecs("%Y-%m-%dT%H:%M:%S", LocalItem->mtime, NULL));
                RemoteTime=CopyStr(RemoteTime, GetDateStrFromSecs("%Y-%m-%dT%H:%M:%S", RemoteItem->mtime, NULL));
                printf("%-30s   remote newer      local:%s    remote:%s\n", Curr->Tag, LocalTime, RemoteTime);
                diff++;
                break;

            case CMP_HASH_MISMATCH:
                printf("%-30s   hash difference\n", Curr->Tag);
                diff++;
                break;

            case CMP_MATCH:
                if (Cmd->Flags & CMD_FLAG_ALL) printf("%-30s   %s match\n", Curr->Tag, MatchType);
                match++;
                break;
            }
        }
        else
        {
            printf("%-30s   local only\n", Curr->Tag);
            diff++;
        }
        Curr=ListGetNext(Curr);
    }

    Curr=ListGetNext(RemoteDir);
    while (Curr)
    {
        RemoteItem=(TFileItem *) Curr->Item;
        Node=ListFindNamedItem(LocalDir, Curr->Tag);
        if (! Node)
        {
            printf("%s   remote only\n", Curr->Tag);
            diff++;
        }
        Curr=ListGetNext(Curr);
    }

    printf("%d items differ %d items match\n", diff, match);


    Destroy(RemoteTime);
    Destroy(LocalTime);
    Destroy(MatchType);
}



int CommandFileItemExists(TCommand *Cmd, TFileStore *FS)
{
int val, result;

				val=FTYPE_ANY;
    		if (Cmd->Flags & CMD_FLAG_FILES_ONLY) val=FTYPE_FILE;
    		if (Cmd->Flags & CMD_FLAG_DIRS_ONLY) val=FTYPE_DIR;

        if (FileStoreItemExists(FS, Cmd->Target, val)) result=TRUE;
				else result=FALSE;

				if (result) UI_Output(0, "'%s' exists", Cmd->Target);
        else UI_Output(0, "'%s' does not exist", Cmd->Target);

return(result);
}



int CommandProcess(TCommand *Cmd, TFileStore *LocalFS, TFileStore *RemoteFS)
{
    char *Tempstr=NULL;
    int result=TRUE;

    CommandActivateTimeout();
    switch (Cmd->Type)
    {
    case CMD_HELP:
        HelpCommand(Cmd->Target);
        break;

    case CMD_DISK_FREE:
        FileStoreOutputDiskQuota(RemoteFS);
        break;

    case CMD_INFO:
        if (StrValid(Cmd->Target))
        {
            if (strncmp(Cmd->Target, "encrypt", 7)==0) FileStoreOutputCipherDetails(RemoteFS, 0);
            else if (strcmp(Cmd->Target, "usage")==0) FileStoreOutputDiskQuota(RemoteFS);
            else if (strcmp(Cmd->Target, "features")==0) FileStoreOutputSupportedFeatures(RemoteFS);
            // else if (strcmp(Cmd->Target, "service")==0) FileStoreOutputServiceInfo(RemoteFS);
            else UI_Output(UI_OUTPUT_ERROR, "unrecognized argument to 'info'. Expected one of 'encrypt', 'usage' or 'features'.");
        }
        else UI_Output(UI_OUTPUT_ERROR, "'info' requires and argument, one of:\n  'encrypt' for connection encryption details\n  'usage' for remote disk usage\n  'features' for supported features of connection/service");
        break;

    case CMD_EXISTS:
				result=CommandFileItemExists(Cmd, RemoteFS);
        break;

    case CMD_LEXISTS:
				result=CommandFileItemExists(Cmd, LocalFS);
        break;

    case CMD_CD:
        result=FileStoreChDir(RemoteFS, Cmd->Target);
        break;

    case CMD_LCD:
        result=FileStoreChDir(LocalFS, Cmd->Target);
        break;

    case CMD_DEL:
        result=CommandGlobAndProcess(RemoteFS, CMD_TYPE_ITEM, Cmd, FileStoreUnlinkItem);
        break;

    case CMD_LDEL:
        result=CommandGlobAndProcess(LocalFS, CMD_TYPE_ITEM, Cmd, FileStoreUnlinkItem);
        break;

    case CMD_MKDIR:
        result=FileStoreMkDir(RemoteFS, Cmd->Target, 0700);
        break;

    case CMD_LMKDIR:
        result=FileStoreMkDir(LocalFS, Cmd->Target, 0700);
        break;

    case CMD_RMDIR:
        result=FileStoreRmDir(RemoteFS, Cmd->Target);
        break;

    case CMD_LRMDIR:
        result=FileStoreRmDir(LocalFS, Cmd->Target);
        break;

    case CMD_RENAME:
        result=CommandGlobAndProcess(RemoteFS, CMD_TYPE_DEST, Cmd, FileStoreRename);
        break;

    case CMD_LRENAME:
        result=CommandGlobAndProcess(LocalFS, CMD_TYPE_DEST, Cmd, FileStoreRename);
        break;

    case CMD_COPY:
        result=CommandGlobAndProcess(RemoteFS, CMD_TYPE_DEST, Cmd, FileStoreCopyFile);
        break;

    case CMD_LCOPY:
        result=CommandGlobAndProcess(LocalFS, CMD_TYPE_DEST, Cmd, FileStoreCopyFile);
        break;

    case CMD_LINK:
        result=CommandGlobAndProcess(RemoteFS, CMD_TYPE_DEST, Cmd, FileStoreLinkPath);
        break;

    case CMD_LLINK:
        result=CommandGlobAndProcess(LocalFS, CMD_TYPE_DEST, Cmd, FileStoreLinkPath);
        break;

    case CMD_LS:
        UI_OutputDirList(RemoteFS, Cmd);
        break;

    case CMD_LLS:
        UI_OutputDirList(LocalFS, Cmd);
        break;

    case CMD_STAT:
        UI_OutputFStat(RemoteFS, Cmd);
        break;

    case CMD_LSTAT:
        UI_OutputFStat(LocalFS, Cmd);
        break;

    case CMD_GET:
    case CMD_MGET:
        HandleEvent(RemoteFS, UI_OUTPUT_DEBUG, "$(filestore) GET $(path)", Cmd->Target, "");
        result=TransferFileCommand(RemoteFS, LocalFS, Cmd);
        break;

    case CMD_PUT:
    case CMD_MPUT:
        HandleEvent(RemoteFS, UI_OUTPUT_DEBUG, "$(filestore) PUT $(path)", Cmd->Target, "");
        result=TransferFileCommand(LocalFS, RemoteFS, Cmd);
        break;

    case CMD_SHOW:
        UI_ShowFile(RemoteFS, LocalFS, Cmd);
        break;

    case CMD_LSHOW:
        UI_ShowFile(LocalFS, NULL, Cmd);
        break;

    case CMD_SHARE:
        Tempstr=FileStoreGetValue(Tempstr, RemoteFS, Cmd->Target, "ShareLink");
        printf("share_url: %s\n", Tempstr);
        break;

    case CMD_PWD:
        printf("%s\n", RemoteFS->CurrDir);
        break;

    case CMD_LPWD:
        printf("%s\n", LocalFS->CurrDir);
        break;

    case CMD_CHMOD:
        result=CommandGlobAndProcess(LocalFS, CMD_TYPE_MODE, Cmd, FileStoreChMod);
        break;

    case CMD_CHPASSWORD:
        if (StrValid(Cmd->Dest)) FileStoreChPassword(RemoteFS, Cmd->Target, Cmd->Dest);
        else FileStoreChPassword(RemoteFS, "", Cmd->Target);
        break;

    case CMD_LOCK:
        result=FileStoreLock(RemoteFS, Cmd->Target, Cmd->Flags);
        break;

    case CMD_LLOCK:
        result=FileStoreLock(LocalFS, Cmd->Target, Cmd->Flags);
        break;

    case CMD_UNLOCK:
        result=FileStoreUnLock(RemoteFS, Cmd->Target);
        break;

    case CMD_LUNLOCK:
        result=FileStoreUnLock(LocalFS, Cmd->Target);
        break;

    case CMD_CRC:
        CommandGetValueGlob(RemoteFS, "crc", Cmd);
        break;

    case CMD_MD5:
        CommandGetValueGlob(RemoteFS, "md5", Cmd);
        break;

    case CMD_SHA1:
        CommandGetValueGlob(RemoteFS, "sha1", Cmd);
        break;

    case CMD_SHA256:
        CommandGetValueGlob(RemoteFS, "sha256", Cmd);
        break;

    case CMD_LCRC:
        CommandGetValueGlob(LocalFS, "crc", Cmd);
        break;

    case CMD_LMD5:
        CommandGetValueGlob(LocalFS, "md5", Cmd);
        break;

    case CMD_LSHA1:
        CommandGetValueGlob(LocalFS, "sha1", Cmd);
        break;

    case CMD_LSHA256:
        CommandGetValueGlob(LocalFS, "sha256", Cmd);
        break;

    case CMD_DIFF:
        CommandDiff(Cmd, LocalFS, RemoteFS);
        break;

    case CMD_CMP:
        Cmd->Flags |= CMD_FLAG_ALL;
        CommandDiff(Cmd, LocalFS, RemoteFS);
        break;

    case CMD_HCMP:
        CommandHashCompare(LocalFS, RemoteFS, Cmd->Target, Cmd->Dest);
        break;
    }

    if (Cmd->Flags & CMD_FLAG_INVERT) result=! result;

    if ((! result) && (Cmd->Flags & CMD_FLAG_ABORT)) result=CMD_ABORT;
    if ((! result) && (Cmd->Flags & CMD_FLAG_QUIT)) result=CMD_QUIT;
    CommandDeactivateTimeout();

    Destroy(Tempstr);

    return(result);
}




void CommandListProcess(const char *Commands, TFileStore *LocalFS, TFileStore *RemoteFS)
{
    char *Tempstr=NULL, *Token=NULL;
    TCommand *Cmd;
    const char *ptr;
    int result;

    if (Settings->Flags & SETTING_VERBOSE) printf("PROCESS COMMANDS: [%s]\n", Commands);

    ptr=GetToken(Commands, ";", &Token, 0);
    while (ptr)
    {
        StripTrailingWhitespace(Token);
        StripLeadingWhitespace(Token);
        Cmd=CommandParse(Token);
        result=CommandProcess(Cmd, LocalFS, RemoteFS);

        if (result == CMD_QUIT) break;
        else if (result == CMD_ABORT)
        {
            HandleEvent(RemoteFS, UI_OUTPUT_ERROR, "ABORT RAISED: $(path)", Token, "");
            break;
        }


        CommandDestroy(Cmd);
        if (Settings->ProcessTimeout > 0)
        {
            if ((GetTime(0) - ProcessStartTime) > Settings->ProcessTimeout) exit(1);
        }
        ptr=GetToken(ptr, ";", &Token, 0);
    }

    Destroy(Tempstr);
    Destroy(Token);
}


void CommandDestroy(void *p_Cmd)
{
    TCommand *Cmd;

    Cmd=(TCommand *) p_Cmd;
    Destroy(Cmd->Target);
    Destroy(Cmd->Dest);
    //ListDestroy(Cmd->Vars, Destroy);
    free(Cmd);
}

#include "global.h"
#include "link_rfu.h"

#if PLATFORM_PC

struct RfuManager gRfu;
struct RfuGameData gHostRfuGameData;
u8 gHostRfuUsername[RFU_USER_NAME_LENGTH];
u8 gWirelessStatusIndicatorSpriteId;

#define STUB_VOID(name, args) void name args {}
#define STUB_BOOL(name, args) bool8 name args { return FALSE; }
#define STUB_BOOL32(name, args) bool32 name args { return FALSE; }
#define STUB_U8(name, args) u8 name args { return 0; }
#define STUB_U32(name, args) u32 name args { return 0; }
#define STUB_S32(name, args) s32 name args { return 0; }

STUB_VOID(WipeTrainerNameRecords, (void));
STUB_VOID(InitRFUAPI, (void));
STUB_VOID(LinkRfu_Shutdown, (void));
STUB_VOID(Rfu_SetBlockReceivedFlag, (u8 linkPlayerId));
STUB_VOID(Rfu_ResetBlockReceivedFlag, (u8 linkPlayerId));
STUB_BOOL32(IsSendingKeysToRfu, (void));
STUB_VOID(StartSendingKeysToRfu, (void));
STUB_VOID(Rfu_SetBerryBlenderLinkCallback, (void));
STUB_U8(Rfu_GetBlockReceivedStatus, (void));
STUB_BOOL32(Rfu_InitBlockSend, (const u8 *src, size_t size));
STUB_VOID(ClearLinkRfuCallback, (void));
STUB_U8(Rfu_GetLinkPlayerCount, (void));
STUB_U8(Rfu_GetMultiplayerId, (void));
STUB_BOOL(Rfu_SendBlockRequest, (u8 type));
STUB_BOOL(IsLinkRfuTaskFinished, (void));
STUB_BOOL(Rfu_IsMaster, (void));
STUB_VOID(Rfu_SetCloseLinkCallback, (void));
STUB_VOID(Rfu_SetLinkStandbyCallback, (void));
STUB_VOID(ResetLinkRfuGFLayer, (void));
STUB_VOID(UpdateWirelessStatusIndicatorSprite, (void));
STUB_VOID(InitRFU, (void));
STUB_BOOL32(RfuMain1, (void));
STUB_BOOL32(RfuMain2, (void));
STUB_BOOL32(RfuHasErrored, (void));
STUB_BOOL32(IsRfuRecvQueueEmpty, (void));
STUB_U32(GetRfuRecvQueueLength, (void));
STUB_VOID(RfuVSync, (void));
STUB_VOID(RfuSetIgnoreError, (bool32 enable));
STUB_U8(RfuGetStatus, (void));
struct RfuGameData *GetHostRfuGameData(void) { return &gHostRfuGameData; }
STUB_VOID(UpdateGameData_GroupLockedIn, (bool8 startedActivity));
STUB_VOID(RfuSetErrorParams, (u32 errorInfo));
STUB_VOID(RfuSetStatus, (u8 status, u16 errorInfo));
STUB_U8(Rfu_SetLinkRecovery, (bool32 enable));
STUB_VOID(CopyHostRfuGameDataAndUsername, (struct RfuGameData *gameData, u8 *username));
STUB_VOID(SetHostRfuGameData, (u8 activity, u32 partnerInfo, bool32 startedActivity));
STUB_VOID(InitializeRfuLinkManager_LinkLeader, (u32 groupMax));
STUB_BOOL32(IsRfuCommunicatingWithAllChildren, (void));
STUB_VOID(LinkRfu_StopManagerAndFinalizeSlots, (void));
STUB_BOOL32(RfuTryDisconnectLeavingChildren, (void));
STUB_BOOL32(HasTrainerLeftPartnersList, (u16 trainerId, const u8 *name));
STUB_VOID(SendRfuStatusToPartner, (u8 status, u16 trainerId, const u8 *name));
STUB_U32(WaitSendRfuStatusToPartner, (u16 trainerId, const u8 *name));
STUB_VOID(RequestDisconnectSlotByTrainerNameAndId, (const u8 *name, u16 id));
STUB_BOOL(LmanAcceptSlotFlagIsNotZero, (void));
STUB_BOOL32(WaitRfuState, (bool32 force));
STUB_VOID(GetOtherPlayersInfoFlags, (void));
STUB_VOID(InitializeRfuLinkManager_JoinGroup, (void));
STUB_VOID(SendLeaveGroupNotice, (void));
STUB_VOID(SaveLinkTrainerNames, (void));
STUB_VOID(LinkRfu_CreateConnectionAsParent, (void));
STUB_VOID(LinkRfu_StopManagerBeforeEnteringChat, (void));
STUB_VOID(UpdateGameData_SetActivity, (u8 activity, u32 partnerInfo, bool32 startedActivity));
STUB_VOID(CreateTask_RfuReconnectWithParent, (const u8 *name, u16 trainerId));
STUB_VOID(SetHostRfuWonderFlags, (bool32 hasNews, bool32 hasCard));
STUB_VOID(ResetHostRfuGameData, (void));
STUB_VOID(SetTradeBoardRegisteredMonInfo, (u32 type, u32 species, u32 level));
STUB_VOID(InitializeRfuLinkManager_EnterUnionRoom, (void));
STUB_VOID(TryConnectToUnionRoomParent, (const u8 *name, struct RfuGameData *parent, u8 activity));
STUB_BOOL32(IsUnionRoomListenTaskActive, (void));
STUB_VOID(Rfu_SendPacket, (void *data));
STUB_BOOL32(PlayerHasMetTrainerBefore, (u16 id, u8 *name));
STUB_VOID(Rfu_DisconnectPlayerById, (u32 playerIdx));
STUB_U8(GetLinkPlayerInfoFlags, (s32 playerId));
STUB_VOID(StopUnionRoomLinkManager, (void));
STUB_BOOL(Rfu_GetCompatiblePlayerData, (struct RfuGameData *gameData, u8 *username, u8 idx));
STUB_BOOL(Rfu_GetWonderDistributorPlayerData, (struct RfuGameData *gameData, u8 *username, u8 idx));
STUB_S32(Rfu_GetIndexOfNewestChild, (u8 bits));
STUB_VOID(CreateTask_RfuIdle, (void));
STUB_VOID(DestroyTask_RfuIdle, (void));
STUB_VOID(ClearRecvCommands, (void));
STUB_VOID(LinkRfu_FatalError, (void));
STUB_BOOL32(Rfu_IsPlayerExchangeActive, (void));
STUB_VOID(Rfu_StopPartnerSearch, (void));
STUB_VOID(RfuSetNormalDisconnectMode, (void));
STUB_VOID(SetUnionRoomChatPlayerData, (u32 numPlayers));
STUB_BOOL32(IsRfuSerialNumberValid, (u32 serialNo));
STUB_BOOL(IsRfuRecoveringFromLinkLoss, (void));
STUB_VOID(RfuRecvQueue_Reset, (struct RfuRecvQueue *queue));
STUB_VOID(RfuSendQueue_Reset, (struct RfuSendQueue *queue));
STUB_VOID(RfuRecvQueue_Enqueue, (struct RfuRecvQueue *queue, u8 *data));
STUB_VOID(RfuSendQueue_Enqueue, (struct RfuSendQueue *queue, u8 *data));
STUB_BOOL(RfuRecvQueue_Dequeue, (struct RfuRecvQueue *queue, u8 *src));
STUB_BOOL(RfuSendQueue_Dequeue, (struct RfuSendQueue *queue, u8 *src));
STUB_VOID(RfuBackupQueue_Enqueue, (struct RfuBackupQueue *queue, const u8 *data));
STUB_BOOL(RfuBackupQueue_Dequeue, (struct RfuBackupQueue *queue, u8 *src));
STUB_VOID(InitHostRfuGameData, (struct RfuGameData *data, u8 activity, bool32 startedActivity, s32 partnerInfo));
STUB_VOID(CreateWirelessStatusIndicatorSprite, (u8 x, u8 y));
STUB_VOID(DestroyWirelessStatusIndicatorSprite, (void));
STUB_VOID(LoadWirelessStatusIndicatorSpriteGfx, (void));

#endif // PLATFORM_PC

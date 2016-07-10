unit _fmMain;

interface

uses
  Winapi.Windows, Winapi.Messages, System.SysUtils, System.Variants, System.Classes, Vcl.Graphics,
  Vcl.Controls, Vcl.Forms, Vcl.Dialogs, Vcl.StdCtrls, Vcl.Buttons, Vcl.ExtCtrls;

type
  TfmMain = class(TForm)
    imgSrc: TImage;
    ScrollBox1: TScrollBox;
    imgDst: TImage;
    Panel2: TPanel;
    btStart: TBitBtn;
    btStop: TBitBtn;
    Timer: TTimer;
    procedure FormDestroy(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    procedure btDecodeClick(Sender: TObject);
    procedure btStartClick(Sender: TObject);
    procedure btStopClick(Sender: TObject);
  private
    FEncoderHandle : pointer;
    FEncoderBuffer : pointer;
    FEncoderBufferSize : integer;
    FSync : pointer;
  private
    FDecoderHandle : pointer;
    FIsPrepared : boolean;
  private
    procedure on_Timer(Sender:TObject);
  public
  end;

var
  fmMain: TfmMain;

implementation

const
  MFX_ERR_NONE                        = 0;
  MFX_ERR_UNKNOWN                     = -1;
  MFX_ERR_NULL_PTR                    = -2;
  MFX_ERR_UNSUPPORTED                 = -3;
  MFX_ERR_MEMORY_ALLOC                = -4;
  MFX_ERR_NOT_ENOUGH_BUFFER           = -5;
  MFX_ERR_INVALID_HANDLE              = -6;
  MFX_ERR_LOCK_MEMORY                 = -7;
  MFX_ERR_NOT_INITIALIZED             = -8;
  MFX_ERR_NOT_FOUND                   = -9;
  MFX_ERR_MORE_DATA                   = -10;
  MFX_ERR_MORE_SURFACE                = -11;
  MFX_ERR_ABORTED                     = -12;
  MFX_ERR_DEVICE_LOST                 = -13;
  MFX_ERR_INCOMPATIBLE_VIDEO_PARAM    = -14;
  MFX_ERR_INVALID_VIDEO_PARAM         = -15;
  MFX_ERR_UNDEFINED_BEHAVIOR          = -16;
  MFX_ERR_DEVICE_FAILED               = -17;
  MFX_ERR_MORE_BITSTREAM              = -18;
  MFX_ERR_INCOMPATIBLE_AUDIO_PARAM    = -19;
  MFX_ERR_INVALID_AUDIO_PARAM         = -20;
  MFX_WRN_IN_EXECUTION                = 1;
  MFX_WRN_DEVICE_BUSY                 = 2;
  MFX_WRN_VIDEO_PARAM_CHANGED         = 3;
  MFX_WRN_PARTIAL_ACCELERATION        = 4;
  MFX_WRN_INCOMPATIBLE_VIDEO_PARAM    = 5;
  MFX_WRN_VALUE_NOT_CHANGED           = 6;
  MFX_WRN_OUT_OF_RANGE                = 7;
  MFX_WRN_FILTER_SKIPPED              = 10;
  MFX_WRN_INCOMPATIBLE_AUDIO_PARAM    = 11;
  MFX_TASK_DONE                       = MFX_ERR_NONE;
  MFX_TASK_WORKING                    = 8;
  MFX_TASK_BUSY                       = 9;

function  OpenEncoder(var AErrorCode:integer; width,height,frameRate,bitRate,gop:integer):pointer;
          cdecl; external 'IntelEncoder.dll';

procedure CloseEncoder(AHandle:pointer);
          cdecl; external 'IntelEncoder.dll';

function  EncodeBitmap(AHandle:pointer; pBitmap:pointer; var sync:pointer):boolean;
          cdecl; external 'IntelEncoder.dll';

function  SyncEncodeResult(AHandle:pointer; sync:pointer; pBuffer:pointer; var ABufferSize:integer; wait:integer):integer;
          cdecl; external 'IntelEncoder.dll';

function  OpenDecoder(var AErrorCode:integer):pointer;
          cdecl; external 'IntelEncoder.dll';

function  PrepareDecoder(AHandle:pointer; pBuffer:pointer; size_of_buffer:integer; var AErrorCode,AWidth,AHeight:integer):boolean;
          cdecl; external 'IntelEncoder.dll';

procedure CloseDecoder(AHandle:pointer);
          cdecl; external 'IntelEncoder.dll';

function  DecodeStreamData(AHandle:pointer; pBuffer:pointer; size_of_buffer:integer):integer;
          cdecl; external 'IntelEncoder.dll';

function  GetBitmap(AHandle:pointer; pBitmap:pointer; wait:integer):boolean;
          cdecl; external 'IntelEncoder.dll';

procedure ScreenShot(AX,AY,AW,AH:integer; ABitmap:TBitmap);
var
  Canvas : TCanvas;
  SrcRect, DstRect : TRect;
begin
  Canvas := TCanvas.Create;
  Canvas.Handle := GetWindowDC(GetDesktopWindow);
  try
    SrcRect := Rect(AX, AY, AW, AH);
    DstRect := Rect( 0,  0, AW, AH);
    ABitmap.Canvas.CopyRect(DstRect, Canvas, SrcRect);
  finally
    ReleaseDC(0, Canvas.Handle);
    Canvas.Free;
  end;
end;

{$R *.dfm}

procedure TfmMain.btDecodeClick(Sender: TObject);
var
  iResult : integer;
begin
  iResult := DecodeStreamData(FDecoderHandle, FEncoderBuffer, FEncoderBufferSize);
  Caption := Format('DecodeStreamData: %d', [iResult]);
end;

procedure TfmMain.btStartClick(Sender: TObject);
begin
  Timer.OnTimer := on_Timer;
end;

procedure TfmMain.btStopClick(Sender: TObject);
begin
  Timer.OnTimer := nil;
end;

procedure TfmMain.FormCreate(Sender: TObject);
var
  iErrorCode : integer;
begin
  FSync := nil;
  FEncoderHandle := nil;
  FDecoderHandle := nil;

  GetMem(FEncoderBuffer, 1024 * 1024);

  imgSrc.Picture.Bitmap.PixelFormat := pf24bit;
  imgSrc.Picture.Bitmap.Width  := 1280;
  imgSrc.Picture.Bitmap.Height :=  720;

  imgDst.Picture.Bitmap.PixelFormat := pf24bit;
  imgDst.Picture.Bitmap.Canvas.Brush.Color := clBlack;

  FIsPrepared := false;

  FEncoderHandle := OpenEncoder(iErrorCode, 1280, 720, 30, 1024, 0);
  if iErrorCode <> 0 then
    raise Exception.Create('Error - OpenEncoder');

  FDecoderHandle := OpenDecoder(iErrorCode);
  if iErrorCode <> 0 then
    raise Exception.Create('Error - OpenDecoder');
end;

procedure TfmMain.FormDestroy(Sender: TObject);
begin
  Timer.OnTimer := nil;

  if FEncoderHandle <> nil then CloseEncoder(FEncoderHandle);
  if FDecoderHandle <> nil then CloseDecoder(FDecoderHandle);

  FreeMem(FEncoderBuffer);
end;

procedure TfmMain.on_Timer(Sender: TObject);
var
  isOk : boolean;
  iErrorCode, iResult, iWidth, iHeight : integer;
begin
  Timer.Enabled := false;
  try
    ScreenShot(0, 0, 1280, 720, imgSrc.Picture.Bitmap);
    isOk := EncodeBitmap(FEncoderHandle, imgSrc.Picture.Bitmap.ScanLine[imgSrc.Picture.Bitmap.Height-1], FSync);
    if isOk = false then Exit;

    iResult := MFX_WRN_DEVICE_BUSY;
    while iResult = MFX_WRN_DEVICE_BUSY do
      iResult := SyncEncodeResult(FEncoderHandle, FSync, FEncoderBuffer, FEncoderBufferSize, 6000);

    if FEncoderBufferSize = 0 then Exit;

    if FIsPrepared = false then begin
      FIsPrepared := true;

      isOk := PrepareDecoder(FDecoderHandle, FEncoderBuffer, FEncoderBufferSize, iResult, iWidth, iHeight);
      if isOk = false then
        raise Exception.Create('Error - PrepareDecoder');

      imgDst.Picture.Bitmap.Width  := iWidth;
      imgDst.Picture.Bitmap.Height := iHeight;
    end else begin
      iResult := DecodeStreamData(FDecoderHandle, FEncoderBuffer, FEncoderBufferSize);
      if iResult <> MFX_ERR_NONE then Exit;

      if GetBitmap(FDecoderHandle, imgDst.Picture.Bitmap.ScanLine[imgDst.Picture.Bitmap.Height-1], 6000) then imgDst.Repaint;
    end;
  finally
    Timer.Enabled := true;
  end;
end;

end.

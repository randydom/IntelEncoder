object fmMain: TfmMain
  Left = 0
  Top = 0
  Caption = 'fmMain'
  ClientHeight = 817
  ClientWidth = 1027
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'Tahoma'
  Font.Style = []
  OldCreateOrder = False
  OnCreate = FormCreate
  OnDestroy = FormDestroy
  PixelsPerInch = 96
  TextHeight = 13
  object imgSrc: TImage
    Left = 4
    Top = 100
    Width = 105
    Height = 105
  end
  object ScrollBox1: TScrollBox
    Left = 0
    Top = 41
    Width = 1027
    Height = 776
    Align = alClient
    TabOrder = 0
    ExplicitTop = 82
    ExplicitHeight = 735
    object imgDst: TImage
      Left = 0
      Top = 0
      Width = 105
      Height = 105
      AutoSize = True
    end
  end
  object Panel2: TPanel
    Left = 0
    Top = 0
    Width = 1027
    Height = 41
    Align = alTop
    BevelOuter = bvNone
    TabOrder = 1
    object btStart: TBitBtn
      Left = 12
      Top = 10
      Width = 75
      Height = 25
      Caption = 'Start'
      TabOrder = 0
      OnClick = btStartClick
    end
    object btStop: TBitBtn
      Left = 93
      Top = 10
      Width = 75
      Height = 25
      Caption = 'Stop'
      TabOrder = 1
      OnClick = btStopClick
    end
  end
  object Timer: TTimer
    Interval = 5
    Left = 36
    Top = 248
  end
end

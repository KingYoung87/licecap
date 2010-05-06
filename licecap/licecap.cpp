#include <stdio.h>
#include <windows.h>
#include <signal.h>


#include "../WDL/lice/lice_lcf.h"

bool g_done=false;

void sigfuncint(int a)
{
  printf("\n\nGot signal!\n");
  g_done=true;
}


typedef struct {
  DWORD   cbSize;
  DWORD   flags;
  HCURSOR hCursor;
  POINT   ptScreenPos;
} pCURSORINFO, *pPCURSORINFO, *pLPCURSORINFO;

static void DoMouseCursor(HDC hdc, HWND h)
{
  // XP+ only

  static BOOL (WINAPI *pGetCursorInfo)(pLPCURSORINFO);
  static bool tr;
  if (!tr)
  {
    tr=true;
    HINSTANCE hUser=LoadLibrary("USER32.dll");
    if (hUser)
      *(void **)&pGetCursorInfo = (void*)GetProcAddress(hUser,"GetCursorInfo");
  }

  if (pGetCursorInfo)
  {
    pCURSORINFO ci={sizeof(ci)};
    pGetCursorInfo(&ci);
    if (ci.flags && ci.hCursor)
    {
      ICONINFO inf={0,};
      GetIconInfo(ci.hCursor,&inf);
      DrawIconEx(hdc,ci.ptScreenPos.x-inf.xHotspot,ci.ptScreenPos.y-inf.yHotspot,ci.hCursor,0,0,0,NULL,DI_NORMAL);
      if (inf.hbmColor) DeleteObject(inf.hbmColor);
      if (inf.hbmMask) DeleteObject(inf.hbmMask);
    }
  }
  else printf("fail cursor\n");
}

int main(int argc, char **argv)
{
  signal(SIGINT,sigfuncint);
  if (argc==4 && !strcmp(argv[1],"-d"))
  {
    LICECaptureDecompressor tc(argv[2]);
    if (tc.IsOpen())
    {
      int x;

      if (strstr(argv[3],".gif"))
      {
        void *wr=NULL;
        for (x=0;!g_done;x++)
        {
          LICE_IBitmap *bm = tc.GetCurrentFrame();
          if (!bm) break;
          tc.NextFrame();

          if (!wr)
          {
            wr=LICE_WriteGIFBegin(argv[3],bm,0,tc.GetTimeToNextFrame(),false);
            if (!wr)
            {
              printf("error writing gif '%s'\n",argv[3]);
              break;
            }
          }
          else
            LICE_WriteGIFFrame(wr,bm,0,0,true,tc.GetTimeToNextFrame());
        }
        if (wr)
        {
          LICE_WriteGIFEnd(wr);
        }
      }
      else for (x=0;!g_done;x++)
      {
        LICE_IBitmap *bm = tc.GetCurrentFrame();
        if (!bm) break;
        tc.NextFrame();
        char buf[512];
        if (1)
        {
          sprintf(buf,"%s%03d.png",argv[3],x);
          LICE_WritePNG(buf,bm);
        }
        else
        {
          sprintf(buf,"%s%03d.gif",argv[3],x);
          LICE_WriteGIF(buf,bm,0,false);
        }

      }
    }
    else printf("Error opening '%s'\n",argv[2]);
  }
  else if ((argc==3||argc==4) && !strcmp(argv[1],"-e"))
  {
    DWORD st = GetTickCount();
    double fr = argc==4 ? atof(argv[3]) : 5.0;
    if (fr < 1.0) fr=1.0;
    fr = 1000.0/fr;

    int x=0;
    HWND h = GetDesktopWindow();
    LICE_SysBitmap bm;
    RECT r;
    GetClientRect(h,&r);
    bm.resize(r.right,r.bottom);
    LICECaptureCompressor *tc = new LICECaptureCompressor(argv[2],r.right,r.bottom);
    if (tc->IsOpen())
    {
      printf("Encoding %dx%d target %.1f fps:\n",r.right,r.bottom,1000.0/fr);

      DWORD lastt=GetTickCount();
      while (!g_done)
      {
        HDC hdc = GetDC(h);
        if (hdc)
        {
          BitBlt(bm.getDC(),0,0,r.right,r.bottom,hdc,0,0,SRCCOPY);
          ReleaseDC(h,hdc);
        }

        DoMouseCursor(bm.getDC(),h);

        DWORD thist = GetTickCount();

        x++;
        printf("Frame: %d (%.1ffps, offs=%d)\r",x,x*1000.0/(thist-st),thist-lastt);
        tc->OnFrame(&bm,thist - lastt);
        lastt = thist;
    
        while (GetTickCount() < (DWORD) (st + x*fr) && !g_done) Sleep(1);
      }
      printf("\nFlushing frames\n");
      st = GetTickCount()-st;
      tc->OnFrame(NULL,0);

      WDL_INT64 outsz=tc->GetOutSize();
      WDL_INT64 intsz = tc->GetInSize();
      delete tc;
      printf("%d %dx%d frames in %.1fs, %.1f fps %.1fMB/s (%.1fMB/s -> %.3fMB/s)\n",x,r.right,r.bottom,st/1000.0,x*1000.0/st,
        r.right*r.bottom*4 * (x*1000.0/st) / 1024.0/1024.0,
        intsz /1024.0/1024.0 / (st/1000.0),
        outsz /1024.0/1024.0 / (st/1000.0));
    }
    else printf("Error opening output\n");

  }
  else 
    printf("usage: licecap [-d file.lcf fnoutbase] | [-e file.lcf [fps]]\n");
  
  return 0;
}
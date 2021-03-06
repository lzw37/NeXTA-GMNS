// DlgCarFollowing.cpp : implementation file
//

#include "stdafx.h"
#include "TLite.h"
#include "DlgCarFollowing.h"


// CDlgCarFollowing dialog
extern CPen s_PenSimulationClock;
IMPLEMENT_DYNAMIC(CDlgCarFollowing, CDialog)

CDlgCarFollowing::CDlgCarFollowing(CWnd* pParent /*=NULL*/)
: CDialog(CDlgCarFollowing::IDD, pParent)
{
	m_MOEType = 0;
	m_bMoveDisplay = false;

	float simulation_horizon = 600;  //second
	m_NumberOfTimeSteps = simulation_horizon;  // 0.1 second as resultion, for 10 min
	m_TimeLeft = 0;
	m_Range = m_NumberOfTimeSteps;
	m_TimeRight = m_NumberOfTimeSteps;  // use 5 days as starting show

	// size 1K*36K*4=144M
	m_VechileCFDataAry = NULL;

	m_SimulationTimeInterval = 0.1f;
	m_FreeflowSpeed = 30.0f;  //30 meter per second = 67.108088761632mph
	m_KJam = 0.140f; // 140 veh/km = 0.14 / meter
	m_MeanWaveSpeed = 6.0f; // in km/h

	m_YUpperBound = 2000;  // 2KM
	m_YLowerBound = 0;
	m_bDataAvailable = false;

}
void CDlgCarFollowing::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CDlgCarFollowing, CDialog)
	ON_WM_PAINT()
	ON_COMMAND(ID_SIMULATION_RUN, &CDlgCarFollowing::OnSimulationRun)
	ON_WM_SIZE()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEWHEEL()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONDOWN()
END_MESSAGE_MAP()


// CDlgCarFollowing message handlers
void CDlgCarFollowing::CarFollowingSimulation()
{

		m_NumberOfTimeSteps = g_Simulation_Time_Horizon*60;
		m_TimeLeft = 0;
		m_Range = m_NumberOfTimeSteps;
		m_TimeRight = m_NumberOfTimeSteps;  // use 5 days as starting show

		m_NumberOfVehicles = 0;

		std::list<DTAVehicle*>::iterator iVehicle;
		for (iVehicle = pDoc->m_VehicleSet.begin(); iVehicle != pDoc->m_VehicleSet.end(); iVehicle++)
		{
			if( ((*iVehicle)->m_VehicleID %m_NumberOfLanes ==0) && (*iVehicle)->m_bComplete && ((*iVehicle)->m_NodeSize>=2))
			{

				for(int i=0; i< (*iVehicle)->m_NodeSize; i++)
				{

				if((*iVehicle)->m_NodeAry[i].LinkNo  == m_SelectedLinkNo )
				{
					m_NumberOfVehicles++;
				}
				}
			}
		}
		
	m_VechileCFDataAry	= AllocateDynamicArray<VehicleCarFollowingData>(m_NumberOfVehicles,m_NumberOfTimeSteps);

		//allocate arrayss
	int t,v;
	m_VehicleDataList.resize(m_NumberOfVehicles);

	for(v=0; v<m_NumberOfVehicles; v++)
	{
		m_VehicleDataList[v].FreeflowSpeed  = m_FreeflowSpeed;
		m_VehicleDataList[v].WaveSpeed = m_MeanWaveSpeed;


			for(t=0; t<m_NumberOfTimeSteps; t++)
		{
			m_VechileCFDataAry[v][t].Distance = 0;

			m_VechileCFDataAry[v][t].Acceration  = 0;
		}



	}
// generating starting time and ending time
	v = 0;
		for (iVehicle = pDoc->m_VehicleSet.begin(); iVehicle != pDoc->m_VehicleSet.end(); iVehicle++)
		{
			if(((*iVehicle)->m_VehicleID %m_NumberOfLanes ==0)&&(*iVehicle)->m_bComplete && ((*iVehicle)->m_NodeSize>=2 ))
			{

				for(int i=1; i< (*iVehicle)->m_NodeSize; i++)  // 0 for departure time
				{
				if((*iVehicle)->m_NodeAry[i].LinkNo  == m_SelectedLinkNo )
				{
				m_VehicleDataList[v].StartTime = (*iVehicle)->m_NodeAry [i-1].ArrivalTimeOnDSN*60;  // minute to second
				m_VehicleDataList[v].EndTime = (*iVehicle)->m_NodeAry [i].ArrivalTimeOnDSN*60;

				TRACE("veh %d,time%d,%d\n",v, m_VehicleDataList[v].StartTime, m_VehicleDataList[v].EndTime);
				v++;


				}
				}
			}
		}


	// load vehicles
	// generate random arrival time
	// for given arrival time, speed = free flow speed;

	// updating 
	for(t=1; t<m_NumberOfTimeSteps; t++)
	{
		for(v=0; v<m_NumberOfVehicles; v++)
		{
			// for active vehicles (with positive speed or positive distance
			if(t>=m_VehicleDataList[v].StartTime && t<= m_VehicleDataList[v].EndTime) 
			{
				//calculate free-flow position
				//xiF(t) = xi(t-τ) + vf(τ)

				m_VechileCFDataAry[v][t].Distance = min(m_YUpperBound, m_VechileCFDataAry[v][t-1].Distance +  m_VehicleDataList[v].FreeflowSpeed);
				TRACE("veh %d,time%d,%f\n",v,t,m_VechileCFDataAry[v][t].Distance);

				//calculate congested position
				if(v>=1)
				{
					//τ = 1/(wkj)
					// δ = 1/kj
					float time_tau_in_sec  = 1 /(m_VehicleDataList[v].WaveSpeed*m_KJam);  
					float spacing = 1.0f/m_KJam;   //1000 meters
					//xiC(t) = xi-1(t-τ) - δ
					int time_t_minus_tau = t - int(time_tau_in_sec+0.5); // need to convert time in second to time in simulation time interval

					if(time_t_minus_tau >=0)  // the leader has not reached destination yet
					{
					float CongestedDistance = m_VechileCFDataAry[v-1][time_t_minus_tau].Distance  - spacing;
					// xi(t) = min(xAF(t), xAC(t))
					if (m_VechileCFDataAry[v][t].Distance  > CongestedDistance && CongestedDistance >= m_VechileCFDataAry[v][t-1].Distance)
						m_VechileCFDataAry[v][t].Distance = CongestedDistance;
					}

				}

			}  // for active vehicle
		} // for each vehicle
	}  // for each time

	m_bDataAvailable = true;
}




void CDlgCarFollowing::OnPaint()
{
	CPaintDC dc(this); // device context for painting

	CRect PlotRect;
	GetClientRect(PlotRect);

	CRect PlotRectOrg = PlotRect;

	if(m_TimeLeft<0)
		m_TimeLeft = 0;
	if(m_TimeRight >=m_Range)
		m_TimeRight=m_Range;
	if(m_TimeRight< m_TimeLeft+10)
		m_TimeRight= m_TimeLeft+10;

	PlotRect.top += 25;
	PlotRect.bottom -= 35;
	PlotRect.left += 60;
	PlotRect.right -= 40;

	DrawTimeSpaceGraph(&dc, m_MOEType, PlotRect,true);

}

void CDlgCarFollowing::DrawTimeSpaceGraph(CPaintDC* pDC,int MOEType, CRect PlotRect, bool LinkTextFlag)
{
	CPen NormalPen(PS_SOLID,2,RGB(0,0,0));
	CPen TimePen(PS_DOT,1,RGB(0,0,0));
	CPen DataPen(PS_SOLID,0,RGB(0,0,0));

	pDC->SetBkMode(TRANSPARENT);


	m_UnitTime = 1;
	if((m_TimeRight - m_TimeLeft)>0)
		m_UnitTime = (float)(PlotRect.right - PlotRect.left)/(m_TimeRight - m_TimeLeft);

	m_UnitDistance = (float)(PlotRect.bottom  - PlotRect.top)/(m_YUpperBound - 	m_YLowerBound);


	pDC->SelectObject(&TimePen);

	int TimeInterval = (m_TimeRight - m_TimeLeft)/10;
	int TimeXPosition;

	char buff[20];
	for(int i=m_TimeLeft;i<=m_TimeRight;i+=TimeInterval)
	{
		if(i == m_TimeLeft)
		{
			pDC->SelectObject(&NormalPen);

			i = int((m_TimeLeft/TimeInterval)+0.5)*TimeInterval; // reset time starting point
		}
		else
			pDC->SelectObject(&DataPen);

		TimeXPosition=(long)(PlotRect.left+(i-m_TimeLeft)*m_UnitTime);

		if(i>= m_TimeLeft)
		{
			if(i/2 <10)
				TimeXPosition-=5;
			else
				TimeXPosition-=3;

			int min, sec;
			sec =  i;
			wsprintf(buff,"%d",sec);
			pDC->TextOut(TimeXPosition,PlotRect.bottom+3,buff);
		}
	}

	pDC->SelectObject(&s_PenSimulationClock);
	if(g_Simulation_Time_Stamp >=m_TimeLeft && g_Simulation_Time_Stamp <= m_TimeRight )
	{
		TimeXPosition=(long)(PlotRect.left+(g_Simulation_Time_Stamp -m_TimeLeft)*m_UnitTime);
		pDC->MoveTo(TimeXPosition,PlotRect.bottom+2);
		pDC->LineTo(TimeXPosition,PlotRect.top);
	}

	if(m_bDataAvailable)
	{
		pDC->SelectObject(&DataPen);
		int t,v;
		for(v=0; v<m_NumberOfVehicles; v++)
		{
			for(t=m_VehicleDataList[v].StartTime; t<=m_VehicleDataList[v].EndTime; t+=1)
			{

				int x=(int)(PlotRect.left+(t-m_TimeLeft)*m_UnitTime);
				int y = PlotRect.bottom - (int)((m_VechileCFDataAry[v][t].Distance *m_UnitDistance)+0.50);

					if(t==m_VehicleDataList[v].StartTime)
					{
						pDC->MoveTo(x,y);

					}else
					{
						pDC->LineTo(x,y);

					}
				
			}
		}
	}



}


void CDlgCarFollowing::OnSimulationRun()
{
	CWaitCursor wait;
	CarFollowingSimulation();
	Invalidate();

}

void CDlgCarFollowing::OnSize(UINT nType, int cx, int cy)
{
	RedrawWindow();

	CDialog::OnSize(nType, cx, cy);

	// TODO: Add your message handler code here
}

void CDlgCarFollowing::OnLButtonDown(UINT nFlags, CPoint point)
{
	m_last_cpoint = point;
	AfxGetApp()->LoadCursor(IDC_MOVENETWORK);
	m_bMoveDisplay = true;


	CDialog::OnLButtonDown(nFlags, point);
	Invalidate();
}

BOOL CDlgCarFollowing::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	int CurrentTime = (int)((pt.x - 60.0f)/m_UnitTime + m_TimeLeft);

	if(zDelta <	 0)
	{
		m_TimeLeft-=(0.2*(CurrentTime-m_TimeLeft));
		m_TimeRight+=(0.2*(m_TimeRight-CurrentTime));
	}
	else
	{
		m_TimeLeft+=(0.2*(CurrentTime-m_TimeLeft));
		m_TimeRight-=(0.2*(m_TimeRight-CurrentTime));
	}

	if(m_TimeLeft < 0)
		m_TimeLeft = 0;

	Invalidate();
	return CDialog::OnMouseWheel(nFlags, zDelta, pt);
}

void CDlgCarFollowing::OnMouseMove(UINT nFlags, CPoint point)
{
	if(m_bMoveDisplay)
	{
		CSize OffSet = point - m_last_cpoint;
		int time_shift = max(1,OffSet.cx/m_UnitTime);
		m_TimeLeft-= time_shift;
		m_TimeRight-= time_shift;

		m_last_cpoint = point;

		Invalidate();
	}
	CDialog::OnMouseMove(nFlags, point);
}

void CDlgCarFollowing::OnLButtonUp(UINT nFlags, CPoint point)
{
	if(m_bMoveDisplay)
	{
		CSize OffSet = point - m_last_cpoint;
		int time_shift = max(1,OffSet.cx/m_UnitTime);
		m_TimeLeft-= time_shift;
		m_TimeRight-= time_shift;

		AfxGetApp()->LoadStandardCursor(IDC_ARROW);
		m_bMoveDisplay = false;
		Invalidate();
	}

	CDialog::OnLButtonUp(nFlags, point);}

void CDlgCarFollowing::OnRButtonDown(UINT nFlags, CPoint point)
{
	m_TimeLeft = 0;
	m_TimeRight = 0;
	int v;

	for(v=0; v<m_NumberOfVehicles; v++)
	{
		if( m_TimeRight< m_VehicleDataList[v].EndTime)
			 m_TimeRight = m_VehicleDataList[v].EndTime ;
	}


	Invalidate();
	CDialog::OnRButtonDown(nFlags, point);
}

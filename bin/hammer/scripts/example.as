
class ExampleWedge : ScriptSolid
{
	GUIData[]@ GetGuiData() const
	{
		return {};
	}

	bool GuiClosed(const dictionary@ dict)
	{
		return false;
	}

	CMapClass@ CreateMapSolid( const BoundBox@ box, TextureAlignment_t align )
	{
		float width = (box.maxs[0] - box.mins[0]) / 2;
		float depth = (box.maxs[1] - box.mins[1]) / 2;
		float height = (box.maxs[2] - box.mins[2]) / 2;

		Vector origin;
		box.GetBoundsCenter(origin);

		CMapFace face;
		CMapSolid@ solid = CMapSolid();
		Vector[] points;
		points.resize(4);

		points[0][0] = origin[0] + width;
		points[0][1] = origin[1] + depth;
		points[0][2] = origin[2] + height;

		points[1][0] = origin[0] + width;
		points[1][1] = origin[1] - depth;
		points[1][2] = origin[2] + height;

		points[2][0] = origin[0] - width;
		points[2][1] = origin[1] - depth;
		points[2][2] = origin[2] + height;

		face.CreateFace(points, 3);
		solid.AddFace(face);

		for (int i = 0; i < 3; i++)
		{
			points[i][2] = origin[2] - height;
		}

		face.CreateFace(points, -3);
		solid.AddFace(face);

		points[0][0] = origin[0] + width;
		points[0][1] = origin[1] + depth;
		points[0][2] = origin[2] - height;

		points[1][0] = origin[0] + width;
		points[1][1] = origin[1] + depth;
		points[1][2] = origin[2] + height;

		points[2][0] = origin[0] - width;
		points[2][1] = origin[1] - depth;
		points[2][2] = origin[2] + height;

		points[3][0] = origin[0] - width;
		points[3][1] = origin[1] - depth;
		points[3][2] = origin[2] - height;

		face.CreateFace(points, 4);
		solid.AddFace(face);

		points[0][0] = origin[0] + width;
		points[0][1] = origin[1] - depth;
		points[0][2] = origin[2] + height;

		points[1][0] = origin[0] + width;
		points[1][1] = origin[1] - depth;
		points[1][2] = origin[2] - height;

		points[2][0] = origin[0] - width;
		points[2][1] = origin[1] - depth;
		points[2][2] = origin[2] - height;

		points[3][0] = origin[0] - width;
		points[3][1] = origin[1] - depth;
		points[3][2] = origin[2] + height;

		face.CreateFace(points, 4);
		solid.AddFace(face);

		points[0][0] = origin[0] + width;
		points[0][1] = origin[1] + depth;
		points[0][2] = origin[2] + height;

		points[1][0] = origin[0] + width;
		points[1][1] = origin[1] + depth;
		points[1][2] = origin[2] - height;

		points[2][0] = origin[0] + width;
		points[2][1] = origin[1] - depth;
		points[2][2] = origin[2] - height;

		points[3][0] = origin[0] + width;
		points[3][1] = origin[1] - depth;
		points[3][2] = origin[2] + height;

		face.CreateFace(points, 4);
		solid.AddFace(face);

		solid.CalcBounds(false);
		solid.InitializeTextureAxes(align, INIT_TEXTURE_ALL | INIT_TEXTURE_FORCE);

		return solid;
	}
};

class ExampleSpike : ScriptSolid
{
	GUIData[]@ GetGuiData() const
	{
		return { GUIData( Label, "Spike" ), GUIData( Divider ), GUIData( TextBox, "Sides", 4 ) }; // Script gui layout
	}

	bool GuiClosed(const dictionary@ dict)
	{
		sides = int(dict["Sides"]); // Value is stored with the same name as TextBox
		return sides <= 32; // return true if we can be created
	}

	float DEG2RAD( float ang )
	{
		return ang * (3.14159265358979323846 / 180.f);
	}

	void polyMake( float x1, float  y1, float x2, float y2, int npoints, float start_ang, Vector[]@ pmPoints )
	{
		int	point;
		float angle = start_ang, angle_delta = 360.0f / float(npoints);
		float xrad = (x2-x1) / 2, yrad = (y2-y1) / 2;

		// make centerpoint for polygon:
		float xCenter = x1 + xrad;
		float yCenter = y1 + yrad;

		for( point = 0; point < npoints; point++, angle += angle_delta )
		{
			if( angle > 360 )
				angle -= 360;

			pmPoints[point][0] = /*rint*/(xCenter + (sin(DEG2RAD(angle)) * xrad));
			pmPoints[point][1] = /*rint*/(yCenter + (cos(DEG2RAD(angle)) * yrad));
		}

		pmPoints[point][0] = pmPoints[0][0];
		pmPoints[point][1] = pmPoints[0][1];
	}

	CMapClass@ CreateMapSolid( const BoundBox@ box, TextureAlignment_t align )
	{
		float fWidth = (box.maxs[0] - box.mins[0]) / 2;
		float fDepth = (box.maxs[1] - box.mins[1]) / 2;
		float fHeight = (box.maxs[2] - box.mins[2]) / 2;
		print("Sides: " + sides + "; Width: " + fWidth + "; Depth: " + fDepth + "; Height: " + fHeight);

		Vector origin;
		box.GetBoundsCenter(origin);

		Vector[] pmPoints;
		pmPoints.resize(64);
		polyMake(origin[0] - fWidth, origin[1] - fDepth, origin[0] + fWidth, origin[1] + fDepth, sides, 0, pmPoints);

		CMapFace NewFace;
		CMapSolid@ pSolid = CMapSolid();

		for(int i = 0; i < sides+1; i++)
		{
			// YWB rounding???
			pmPoints[i][2] = /*rint*/(origin[2] - fHeight);
		}

		NewFace.CreateFace(pmPoints, -sides);
		pSolid.AddFace(NewFace);

		// other sides
		Vector[] Points;
		Points.resize(3);

		// get centerpoint
		Points[0][0] = origin[0];
		Points[0][1] = origin[1];
		// YWB rounding???
		Points[0][2] = rint(origin[2] + fHeight);

		for(int i = 0; i < sides; i++)
		{
			Points[1][0] = pmPoints[i][0];
			Points[1][1] = pmPoints[i][1];
			Points[1][2] = pmPoints[i][2];

			Points[2][0] = pmPoints[i+1][0];
			Points[2][1] = pmPoints[i+1][1];
			Points[2][2] = pmPoints[i+1][2];

			NewFace.CreateFace(Points, 3);
			pSolid.AddFace(NewFace);
		}

		pSolid.CalcBounds(false);
		pSolid.InitializeTextureAxes(align, INIT_TEXTURE_ALL | INIT_TEXTURE_FORCE);
		return pSolid;
	}

	int sides;
};

void RegisterCallback()
{
	RegisterScriptSolid("script_wedge", ExampleWedge());
	RegisterScriptSolid("script_spike", ExampleSpike());
}
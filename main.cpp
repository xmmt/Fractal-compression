#include "CImg.h"
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
using namespace std;
using namespace cimg_library;

class block
{
public:
	uint16_t x, y;
	uint8_t max, min;
	float s, o;
};

class fdata
{
public:
	vector < vector < block > > b;
	int w, h, c, rs, ds;
};

ostream& operator<<(ostream& out, const fdata& f)
{
	out.write((char*)&f.w, sizeof(f.w));
	out.write((char*)&f.h, sizeof(f.h));
	out.write((char*)&f.c, sizeof(f.c));
	out.write((char*)&f.rs, sizeof(f.rs));
	out.write((char*)&f.ds, sizeof(f.ds));
	int ii = f.b.size();
	int jj = f.b[0].size();
	out.write((char*)&ii, sizeof(ii));
	out.write((char*)&jj, sizeof(jj));
	for (int i = 0; i < f.b.size(); ++i)
		for (int j = 0; j < f.b[i].size(); ++j)
		{
			out.write((char*)&f.b[i][j].x, sizeof(f.b[i][j].x));
			out.write((char*)&f.b[i][j].y, sizeof(f.b[i][j].y));
			out.write((char*)&f.b[i][j].max, sizeof(f.b[i][j].max));
			out.write((char*)&f.b[i][j].min, sizeof(f.b[i][j].min));
			out.write((char*)&f.b[i][j].s, sizeof(f.b[i][j].s));
			out.write((char*)&f.b[i][j].o, sizeof(f.b[i][j].o));
		}
	return out;
}

istream& operator>>(istream& in, fdata& f)
{
	in.read((char*)&f.w, sizeof(f.w));
	in.read((char*)&f.h, sizeof(f.h));
	in.read((char*)&f.c, sizeof(f.c));
	in.read((char*)&f.rs, sizeof(f.rs));
	in.read((char*)&f.ds, sizeof(f.ds));
	int ii, jj;
	in.read((char*)&ii, sizeof(ii));
	in.read((char*)&jj, sizeof(jj));
	f.b = vector < vector < block > >(ii, vector < block >(jj));
	for (int i = 0; i < f.b.size(); ++i)
		for (int j = 0; j < f.b[i].size(); ++j)
		{
			in.read((char*)&f.b[i][j].x, sizeof(f.b[i][j].x));
			in.read((char*)&f.b[i][j].y, sizeof(f.b[i][j].y));
			in.read((char*)&f.b[i][j].max, sizeof(f.b[i][j].max));
			in.read((char*)&f.b[i][j].min, sizeof(f.b[i][j].min));
			in.read((char*)&f.b[i][j].s, sizeof(f.b[i][j].s));
			in.read((char*)&f.b[i][j].o, sizeof(f.b[i][j].o));
		}
	return in;
}

class picture
{
public:
	vector < vector < vector < int > > > p;
	int w, h, c;

	picture(int w, int h, int c = 1)
		: w(w), h(h), c(c)
	{
		p = vector < vector < vector < int > > >(c, vector < vector < int > >(w + 100, vector < int >(h + 100, 0)));
	}

	picture(CImg < uint8_t > &img)
	{
		set(img);
	}

	void set(CImg < uint8_t > &img)
	{
		w = img.width();
		h = img.height();
		c = img.spectrum();
		int sz = w * h;
		p = vector < vector < vector < int > > >(c, vector < vector < int > >(w + 100, vector < int >(h + 100, 0)));
		for (int x = 0; x < w; ++x)
			for (int y = 0; y < h; ++y)
				for (int z = 0, add = 0; z < c; ++z, add += sz)
				{
					p[z][x][y] = *(img.data() + y * w + x + add);
					if (p[z][x][y] == 0)
						p[z][x][y] = 1;
				}
	}

	CImg < uint8_t > get()
	{
		int sz = w * h;
		CImg < UINT8 > img(w, h, 1, 3);
		for (int x = 0; x < w; ++x)
			for (int y = 0; y < h; ++y)
				for (int z = 0, add = 0; z < c; ++z, add += sz)
					*(img.data() + y * w + x + add) = p[z][x][y];
		return img;
	}
};

class fcoder
{
public:
	static fdata encode(picture &p, int rank_size = 4, int step = 4)
	{
		fdata code;
		int w = code.w = p.w;
		int h = code.h = p.h;
		int c = code.c = p.c;
		int domain_size = rank_size * 2;
		int N = rank_size * rank_size;
		code.rs = rank_size;
		code.ds = domain_size;
		code.b.resize(c);
		vector < vector < int > > dom(rank_size + 100, vector < int >(rank_size + 100));
		for (int channel = 0; channel < c; ++channel)
		{
			for (int rx = 0; rx < w; rx += rank_size)
				for (int ry = 0; ry < h; ry += rank_size)
				{
					bool fl = true;
					float bq = 1e9, q, s, o;
					int r = 0, r2 = 0, d, d2, rd, t;
					block best;
					int max = -1000, min = 1000;
					for (int x = 0; x < rank_size; ++x)
						for (int y = 0; y < rank_size; ++y)
						{
							t = p.p[channel][rx + x][ry + y];
							r += t;
							r2 += t * t;
							if (t > max) max = t;
							if (t < min) min = t;
						}
					best.max = max;
					best.min = min;
					for (int dx = 0; dx + domain_size <= w; dx += step)
						for (int dy = 0; dy + domain_size <= h; dy += step)
						{
							d = d2 = rd = 0;
							for (int x = 0; x < rank_size; ++x)
								for (int y = 0; y < rank_size; ++y)
									dom[x][y] = 0;
							for (int x = 0; x < domain_size; ++x)
								for (int y = 0; y < domain_size; ++y)
									dom[x / 2][y / 2] += p.p[channel][dx + x][dy + y];
							for (int x = 0; x < rank_size; ++x)
								for (int y = 0; y < rank_size; ++y)
									dom[x][y] /= 4;
							for (int x = 0; x < rank_size; ++x)
								for (int y = 0; y < rank_size; ++y)
								{
									t = dom[x][y];
									d += t;
									d2 += t * t;
									rd += t * p.p[channel][rx + x][ry + y];
								}
							s = float(N*rd - r * d) / float(N*d2 - d * d);
							o = float(r - s * d) / float(N);
							q = s * s*d2 + N * o*o + r2 - 2 * s*rd + 2 * s*o*d - 2 * o*r;
							if (fl || (!fl && q < bq))
							{
								fl = false;
								bq = q;
								best.x = dx;
								best.y = dy;
								best.o = o;
								best.s = s;
							}
						}
					code.b[channel].push_back(best);
				}
		}
		return code;
	}

	static picture decode(fdata &code)
	{
		int w = code.w, h = code.h, c = code.c;
		int rank_size = code.rs;
		int domain_size = code.ds;
		picture p(w, h, c);
		vector < vector < int > > dom(rank_size + 100, vector < int >(rank_size + 100));
		vector < vector < int > > pic(w + 100, vector < int >(h + 100));
		for (int iter = 0; iter < 100; ++iter)
			for (int channel = 0; channel < c; ++channel)
			{
				int k = 0;
				for (int rx = 0; rx < w; rx += rank_size)
					for (int ry = 0; ry < h; ry += rank_size)
					{
						for (int x = 0; x < rank_size; ++x)
							for (int y = 0; y < rank_size; ++y)
								dom[x][y] = 0;
						int dx = code.b[channel][k].x, dy = code.b[channel][k].y;
						float s = code.b[channel][k].s, o = code.b[channel][k].o;
						for (int x = 0; x < domain_size; ++x)
							for (int y = 0; y < domain_size; ++y)
								dom[x / 2][y / 2] += p.p[channel][dx + x][dy + y];
						for (int x = 0; x < rank_size; ++x)
							for (int y = 0; y < rank_size; ++y)
							{
								pic[rx + x][ry + y] = int(float(dom[x][y]) / 4.0 * s + o);
								if (pic[rx + x][ry + y] > code.b[channel][k].max) pic[rx + x][ry + y] = code.b[channel][k].max;
								if (pic[rx + x][ry + y] < code.b[channel][k].min) pic[rx + x][ry + y] = code.b[channel][k].min;
							}
						k++;
					}
				for (int x = 0; x < w; ++x)
					for (int y = 0; y < h; ++y)
						p.p[channel][x][y] = pic[x][y];
			}
		return p;
	}
};

int main()
{
	CImg < uint8_t > i1("pic.bmp");
	picture p1(i1);
	auto d = fcoder::encode(p1, 4, 1);
	ofstream out("out", ios::binary);
	out << d;
	out.close();
	ifstream in("out", ios::binary);
	fdata f;
	in >> f;
	auto p2 = fcoder::decode(f);
	auto i2 = p2.get();
	CImgDisplay d1(i1), d2(i2);
	while (!d1.is_closed() && !d2.is_closed())
	{
		d1.wait();
		d2.wait();
	}
	return 0;
}
struct test{
	int c;
	int p2;
	int arr[5][5];
	int d;
};

struct test2{
	char arr[5][5];
	char arr2[4][4];
};

struct test t1;
int oo[6][6];
int t2;

int fun(int x){
	struct test2 t3;
	t1.d = oo[2][2];
	oo[2][2] = t3.arr2[3][3];
}

int main(){
	int b;
}

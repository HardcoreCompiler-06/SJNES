package nguyenducan;

public class Advance5 {
    public boolean kiemTraDoiXung(int number) {
        StringBuilder xau = new StringBuilder();
        String str = number + "";
        xau.append(str);
        String check = xau.reverse().toString();
        return str.equals(check);
    }
}
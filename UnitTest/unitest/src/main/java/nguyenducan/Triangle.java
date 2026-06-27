package nguyenducan;

public class Triangle {
    public int number1;
    public int number2;
    public int number3;

    public Triangle(int number1, int number2, int number3) {
        this.number1 = number1;
        this.number2 = number2;
        this.number3 = number3;
    }

    public int getNumber1() { return number1; }
    public int getNumber2() { return number2; }
    public int getNumber3() { return number3; }

    public int maxLength() {
        if (number1 >= number2)
            if (number1 > number3)
                return number1;
            else
                return number3;
        if (number2 > number3)
            return number2;
        else
            return number3;
    }
}
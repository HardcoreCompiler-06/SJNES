package nguyenducan;

public class Sort1 {
    public int number1;
    public int number2;

    public int getNumber1() { return number1; }
    public void setNumber1(int number1) { this.number1 = number1; }
    public int getNumber2() { return number2; }
    public void setNumber2(int number2) { this.number2 = number2; }

    public void sortAsc() {
        if (number1 > number2) {
            int temp = number1;
            number1 = number2;
            number2 = temp;
        }
    }
}
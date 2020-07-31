using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace SIGameIRL
{
    /// <summary>
    /// Логика взаимодействия для MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        public List<UserScoreControl> userScores = new List<UserScoreControl>();
        public QuestionPack questionPack;

        public MainWindow()
        {
            InitializeComponent();

            for (int i = 0; i < 8; i++)
			{
                var a = new UserScoreControl("Name" + i.ToString(), "Score: " + i.ToString());
                Grid.SetColumn(a, i);
                userScores.Add(a);
                grScores.ColumnDefinitions.Add(new ColumnDefinition());
                grScores.Children.Add(a);
            }

            OpenFileDialog ofd = new OpenFileDialog();
            ofd.Filter = "*.siq|*.siq";
            ofd.ShowDialog();
			if (!string.IsNullOrEmpty(ofd.FileName))
			{
                questionPack = new QuestionPack(ofd.FileName);
			}
		}

        public void Update()
		{
            double usWidth = this.ActualWidth / userScores.Count;
            foreach (var us in userScores)
			{
                us.Width = usWidth;
                us.Update();
			}
		}

		private void Window_SizeChanged(object sender, SizeChangedEventArgs e)
        {
            Update();
        }
	}
}

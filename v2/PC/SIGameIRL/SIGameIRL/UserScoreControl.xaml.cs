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
	/// Логика взаимодействия для UserScore.xaml
	/// </summary>
	public partial class UserScoreControl : UserControl
	{
		public string UserName { get; set; }
		public string UserScore { get; set; }

		public UserScoreControl(string UserName, string UserScore)
		{
			InitializeComponent();
			this.UserName = UserName;
			this.UserScore = UserScore;
			Update();
		}

		public void Update()
		{
			laName.Content = UserName;
			laScore.Content = UserScore;
		}
	}
}
